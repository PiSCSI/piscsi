package server

import (
	"archive/zip"
	"bytes"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi-web/internal/config"
	"github.com/piscsi/piscsi-web/internal/server/testutil"
	pb "github.com/piscsi/piscsi-web/proto"
)

const lsarFixture = `{
  "lsarContents": [
    {"XADFileName":"folder","XADFileSize":0,"XADIsDirectory":true},
    {"XADFileName":"folder/system.hds","XADFileSize":1048576},
    {"XADFileName":"folder/system.hds.properties","XADFileSize":64},
    {"XADFileName":"folder/._system.hds","XADFileSize":32,"XADIsResourceFork":true},
    {"XADFileName":"../escape.hds","XADFileSize":1},
    {"XADFileName":"notes.txt","XADFileSize":12}
  ]
}`

func writeExecutable(t *testing.T, path, contents string) {
	t.Helper()
	if err := os.WriteFile(path, []byte(contents), 0o755); err != nil {
		t.Fatal(err)
	}
}

func TestArchiveSuffixes(t *testing.T) {
	for _, suffix := range []string{"zip", ".SIT", "tar", "gz", "7Z"} {
		if !isArchiveSuffix(suffix) {
			t.Errorf("isArchiveSuffix(%q) = false", suffix)
		}
	}
	for _, suffix := range []string{"iso", "tgz", "rar", ""} {
		if isArchiveSuffix(suffix) {
			t.Errorf("isArchiveSuffix(%q) = true", suffix)
		}
	}
}

func TestLSARMemberAcceptsNumericFlagsAndStringSize(t *testing.T) {
	input := `{
	  "XADFileName":"disk.hds",
	  "XADFileSize":"4096",
	  "XADIsDirectory":0,
	  "XADIsResourceFork":1,
	  "XADIsLink":"false"
	}`
	var member lsarMember
	if err := json.Unmarshal([]byte(input), &member); err != nil {
		t.Fatal(err)
	}
	if member.Name != "disk.hds" || member.Size != 4096 ||
		bool(member.IsDirectory) || !bool(member.IsResourceFork) || bool(member.IsLink) {
		t.Fatalf("decoded member = %#v", member)
	}
}

func TestInspectArchiveMembersUsesNativeZipFallback(t *testing.T) {
	var archive bytes.Buffer
	writer := zip.NewWriter(&archive)
	member, err := writer.Create("folder/disk.hds")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := member.Write([]byte("image")); err != nil {
		t.Fatal(err)
	}
	if err := writer.Close(); err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(t.TempDir(), "disk.zip")
	if err := os.WriteFile(path, archive.Bytes(), 0o644); err != nil {
		t.Fatal(err)
	}
	t.Setenv("PATH", t.TempDir())

	members, err := inspectArchiveMembers(path)
	if err != nil {
		t.Fatal(err)
	}
	if len(members) != 1 || members[0].Name != "folder/disk.hds" || members[0].Size != 5 {
		t.Fatalf("native ZIP members = %#v", members)
	}
}

func TestGetImageFilesIncludesArchivesAndCachesInspection(t *testing.T) {
	root := t.TempDir()
	imageDir := filepath.Join(root, "images")
	binDir := filepath.Join(root, "bin")
	if err := os.MkdirAll(imageDir, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(binDir, 0o755); err != nil {
		t.Fatal(err)
	}
	archivePath := filepath.Join(imageDir, "software.7z")
	if err := os.WriteFile(archivePath, []byte("archive"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(imageDir, "disk.hds"), []byte("image"), 0o644); err != nil {
		t.Fatal(err)
	}

	counter := filepath.Join(root, "lsar-calls")
	writeExecutable(t, filepath.Join(binDir, "lsar"), "#!/bin/sh\nprintf x >> \""+counter+"\"\nprintf '%s\\n' '"+lsarFixture+"'\n")
	t.Setenv("PATH", binDir+string(os.PathListSeparator)+os.Getenv("PATH"))

	server := &Server{config: &config.Config{BaseDir: imageDir}}
	mapping := map[string]pb.PbDeviceType{"hds": pb.PbDeviceType_SCHD}
	for range 2 {
		files, _ := server.getImageFiles(mapping, nil)
		if len(files) != 2 {
			t.Fatalf("getImageFiles() returned %d files, want 2", len(files))
		}
		var archive map[string]interface{}
		for _, file := range files {
			if file["Name"] == "software.7z" {
				archive = file
			}
		}
		if archive == nil || archive["IsArchive"] != true {
			t.Fatalf("archive entry = %#v", archive)
		}
		if archive["Size"] != int64(7) || archive["DisplaySize"] != "7 bytes" {
			t.Errorf("archive size = (%v, %v), want (7, 7 bytes)", archive["Size"], archive["DisplaySize"])
		}
		members := archive["ArchiveContents"].([]archiveMember)
		if len(members) != 3 {
			t.Fatalf("archive members = %#v, want 3 displayable members", members)
		}
		if members[1].Path != "folder/system.hds.properties" || !members[1].IsPropertiesFile {
			t.Errorf("properties member = %#v", members[1])
		}
		if members[0].RelatedPropertiesFile != "folder/system.hds.properties" {
			t.Errorf("related properties = %q", members[0].RelatedPropertiesFile)
		}
	}
	calls, err := os.ReadFile(counter)
	if err != nil {
		t.Fatal(err)
	}
	if string(calls) != "x" {
		t.Fatalf("lsar calls = %q, want one cached call", calls)
	}

	if err := os.WriteFile(archivePath, []byte("changed archive"), 0o644); err != nil {
		t.Fatal(err)
	}
	server.getImageFiles(mapping, nil)
	calls, err = os.ReadFile(counter)
	if err != nil {
		t.Fatal(err)
	}
	if string(calls) != "xx" {
		t.Fatalf("lsar calls after archive change = %q, want cache invalidation", calls)
	}
}

func TestHandleFilesExtractImageSupportsSubdirectoriesAndProperties(t *testing.T) {
	gin.SetMode(gin.TestMode)
	root := t.TempDir()
	imageDir := filepath.Join(root, "images")
	configDir := filepath.Join(root, "properties")
	binDir := filepath.Join(root, "bin")
	archiveDir := filepath.Join(imageDir, "archives")
	for _, directory := range []string{archiveDir, configDir, binDir} {
		if err := os.MkdirAll(directory, 0o755); err != nil {
			t.Fatal(err)
		}
	}
	if err := os.WriteFile(filepath.Join(archiveDir, "software.sit"), []byte("archive"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(imageDir, "unrelated.properties"), []byte("existing"), 0o644); err != nil {
		t.Fatal(err)
	}

	writeExecutable(t, filepath.Join(binDir, "lsar"), "#!/bin/sh\nprintf '%s\\n' '"+lsarFixture+"'\n")
	writeExecutable(t, filepath.Join(binDir, "unar"), `#!/bin/sh
out=
while [ "$#" -gt 0 ]; do
  if [ "$1" = "-output-directory" ]; then
    shift
    out="$1"
  fi
  shift
done
mkdir -p "$out/folder"
printf image > "$out/folder/system.hds"
printf properties > "$out/folder/system.hds.properties"
`)
	t.Setenv("PATH", binDir+string(os.PathListSeparator)+os.Getenv("PATH"))

	server := &Server{
		config:       &config.Config{BaseDir: imageDir, ConfigDir: configDir},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
	}
	form := url.Values{
		"archive_file":    {"archives/software.sit"},
		"archive_members": {"folder/system.hds|folder/system.hds.properties"},
	}
	recorder := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(recorder)
	context.Request = httptest.NewRequest(http.MethodPost, "/files/extract_image", strings.NewReader(form.Encode()))
	context.Request.Header.Set("Content-Type", "application/x-www-form-urlencoded")

	server.handleFilesExtractImage(context)

	if recorder.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", recorder.Code, recorder.Body.String())
	}
	for _, path := range []string{
		filepath.Join(imageDir, "folder", "system.hds"),
		filepath.Join(configDir, "folder", "system.hds.properties"),
	} {
		if _, err := os.Stat(path); err != nil {
			t.Errorf("expected extracted file %s: %v", path, err)
		}
	}
	if _, err := os.Stat(filepath.Join(imageDir, "folder", "system.hds.properties")); !os.IsNotExist(err) {
		t.Errorf("properties file was left in image directory: %v", err)
	}

	session := testutil.GetSessionFromResponse(t, recorder.Result(), server.sessionStore, context.Request)
	message, _ := GetFlashesForTemplate(session)
	if message != "Extracted 2 file(s)" {
		t.Errorf("message = %q, want %q", message, "Extracted 2 file(s)")
	}
}

func TestRequestedArchiveMembersRejectsUnknownMembers(t *testing.T) {
	available := []archiveMember{{Path: "disk.hds"}}
	if _, err := requestedArchiveMembers("", available); err == nil {
		t.Fatal("empty member list was accepted")
	}
	if _, err := requestedArchiveMembers("../disk.hds", available); err == nil {
		t.Fatal("unknown member was accepted")
	}
}
