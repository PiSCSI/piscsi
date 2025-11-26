package server

import (
	"archive/zip"
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi-web/internal/config"
	"github.com/piscsi/piscsi-web/internal/driveprops"
	"github.com/piscsi/piscsi-web/internal/server/testutil"
	pb "github.com/piscsi/piscsi-web/proto"
)

func TestImageSuffixesUsesDaemonMapping(t *testing.T) {
	mapping := map[string]pb.PbDeviceType{
		"tap":    pb.PbDeviceType_SCTP,
		"iso":    pb.PbDeviceType_SCCD,
		"hdr":    pb.PbDeviceType_SCRM,
		"hd1":    pb.PbDeviceType_SCHD,
		"hda":    pb.PbDeviceType_SCHD,
		"mos":    pb.PbDeviceType_SCMO,
		"ignore": pb.PbDeviceType_UNDEFINED,
	}

	want := []string{"hd1", "hda", "hdr", "mos", "iso", "tap"}
	if got := imageSuffixes(mapping); !reflect.DeepEqual(got, want) {
		t.Fatalf("imageSuffixes() = %v, want %v", got, want)
	}
}

func TestCreatableImageSuffixesUsesDaemonMapping(t *testing.T) {
	mapping := map[string]pb.PbDeviceType{
		"tap": pb.PbDeviceType_SCTP,
		"iso": pb.PbDeviceType_SCCD,
		"hdr": pb.PbDeviceType_SCRM,
		"hd1": pb.PbDeviceType_SCHD,
		"hda": pb.PbDeviceType_SCHD,
		"hds": pb.PbDeviceType_SCHD,
		"HDI": pb.PbDeviceType_SCHD,
		"nhd": pb.PbDeviceType_SCHD,
		"mos": pb.PbDeviceType_SCMO,
	}

	got := creatableImageSuffixes(mapping)
	gotSuffixes := make([]string, 0, len(got))
	for _, imageType := range got {
		gotSuffixes = append(gotSuffixes, imageType.Suffix)
	}
	want := []string{"hds", "hda", "hd1", "hdr", "mos", "tap"}
	if !reflect.DeepEqual(gotSuffixes, want) {
		t.Fatalf("creatableImageSuffixes() = %v, want %v", gotSuffixes, want)
	}
}

func TestISOFormatArgsMatchesPythonWorkflow(t *testing.T) {
	root := t.TempDir()
	templatesDir := filepath.Join(root, "web", "templates")
	if err := os.MkdirAll(templatesDir, 0o755); err != nil {
		t.Fatal(err)
	}
	mapPath := filepath.Join(root, "web", "genisoimage_hfs_resource_fork_map.txt")
	if err := os.WriteFile(mapPath, []byte(".* Raw 'ttxt' 'BINA'"), 0o644); err != nil {
		t.Fatal(err)
	}
	server := &Server{config: &config.Config{TemplatesDir: templatesDir}}

	tests := map[string][]string{
		"HFS":              {"-hfs", "-map", mapPath},
		"ISO-9660 Level 1": {"-iso-level", "1"},
		"ISO-9660 Level 2": {"-iso-level", "2"},
		"ISO-9660 Level 3": {"-iso-level", "3"},
		"Joliet":           {"-J"},
		"Rock Ridge":       {"-r"},
	}
	for isoType, want := range tests {
		got, ok := server.isoFormatArgs(isoType)
		if !ok || !reflect.DeepEqual(got, want) {
			t.Errorf("isoFormatArgs(%q) = (%v, %v), want (%v, true)", isoType, got, ok, want)
		}
	}
	if got, ok := server.isoFormatArgs("UDF"); ok || got != nil {
		t.Errorf("isoFormatArgs(UDF) = (%v, %v), want (nil, false)", got, ok)
	}
}

func TestDownloadISOSourceExpandsZip(t *testing.T) {
	var archive bytes.Buffer
	writer := zip.NewWriter(&archive)
	member, err := writer.Create("folder/readme.txt")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := member.Write([]byte("hello")); err != nil {
		t.Fatal(err)
	}
	if err := writer.Close(); err != nil {
		t.Fatal(err)
	}

	downloadServer := httptest.NewServer(http.HandlerFunc(func(response http.ResponseWriter, request *http.Request) {
		response.Header().Set("Content-Type", "application/zip")
		_, _ = response.Write(archive.Bytes())
	}))
	defer downloadServer.Close()

	server := &Server{config: &config.Config{MaxFileSize: int64(archive.Len() + 1)}}
	sourcePath, fileName, cleanup, err := server.downloadISOSource(context.Background(), downloadServer.URL+"/software.zip")
	if cleanup != nil {
		defer cleanup()
	}
	if err != nil {
		t.Fatal(err)
	}
	if fileName != "software.zip" {
		t.Fatalf("fileName = %q, want software.zip", fileName)
	}
	content, err := os.ReadFile(filepath.Join(sourcePath, "folder", "readme.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if string(content) != "hello" {
		t.Fatalf("expanded content = %q, want hello", content)
	}
	if _, err := os.Stat(filepath.Join(sourcePath, fileName)); !os.IsNotExist(err) {
		t.Fatalf("downloaded ZIP was not removed after expansion: %v", err)
	}
}

func TestExpandZipForISOKeepsMacZipArchive(t *testing.T) {
	root := t.TempDir()
	archivePath := filepath.Join(root, "mac.zip")
	var archive bytes.Buffer
	writer := zip.NewWriter(&archive)
	member, err := writer.Create("__MACOSX/XtraStuf.mac")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := member.Write([]byte("resource fork")); err != nil {
		t.Fatal(err)
	}
	if err := writer.Close(); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(archivePath, archive.Bytes(), 0o644); err != nil {
		t.Fatal(err)
	}

	expanded, err := expandZipForISO(archivePath, root)
	if err != nil {
		t.Fatal(err)
	}
	if expanded {
		t.Fatal("MacZip archive was expanded")
	}
}

func TestHandleFilesCreateISOLocalFile(t *testing.T) {
	gin.SetMode(gin.TestMode)
	root := t.TempDir()
	imageDir := filepath.Join(root, "images")
	binDir := filepath.Join(root, "bin")
	if err := os.MkdirAll(imageDir, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(binDir, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(imageDir, "software.hds"), []byte("software"), 0o644); err != nil {
		t.Fatal(err)
	}
	fakeGenisoimage := "#!/bin/sh\nwhile [ \"$1\" != \"-o\" ]; do shift; done\nshift\n: > \"$1\"\n"
	if err := os.WriteFile(filepath.Join(binDir, "genisoimage"), []byte(fakeGenisoimage), 0o755); err != nil {
		t.Fatal(err)
	}
	t.Setenv("PATH", binDir+string(os.PathListSeparator)+os.Getenv("PATH"))

	server := &Server{
		config:       &config.Config{BaseDir: imageDir, TemplatesDir: filepath.Join(root, "web", "templates")},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
	}
	form := url.Values{"file": {"software.hds"}, "type": {"Joliet"}}
	recorder := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(recorder)
	context.Request = httptest.NewRequest(http.MethodPost, "/files/create_iso", strings.NewReader(form.Encode()))
	context.Request.Header.Set("Content-Type", "application/x-www-form-urlencoded")

	server.handleFilesCreateISO(context)

	if recorder.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", recorder.Code, recorder.Body.String())
	}
	if _, err := os.Stat(filepath.Join(imageDir, "software.hds.iso")); err != nil {
		t.Fatalf("ISO was not created: %v", err)
	}
	session := testutil.GetSessionFromResponse(t, recorder.Result(), server.sessionStore, context.Request)
	message, _ := GetFlashesForTemplate(session)
	if message != "CD-ROM image software.hds.iso with type Joliet was created." {
		t.Fatalf("message = %q", message)
	}
}

func TestGetImageFilesUsesLocalDirectoryAndDaemonMapping(t *testing.T) {
	imageDir := t.TempDir()
	nestedDir := filepath.Join(imageDir, "nested")
	if err := os.Mkdir(nestedDir, 0o755); err != nil {
		t.Fatal(err)
	}
	for name, content := range map[string][]byte{
		"system.hd1":                          make([]byte, 2*1024*1024),
		filepath.Join("nested", "disc.toast"): make([]byte, 3*1024*1024),
		"notes.txt":                           []byte("not a recognized image"),
	} {
		if err := os.WriteFile(filepath.Join(imageDir, name), content, 0o644); err != nil {
			t.Fatal(err)
		}
	}

	server := &Server{
		config: &config.Config{BaseDir: imageDir},
	}
	mapping := map[string]pb.PbDeviceType{
		"hd1":   pb.PbDeviceType_SCHD,
		"toast": pb.PbDeviceType_SCCD,
	}

	attached := map[string]struct{}{"nested/disc.toast": {}}
	files, filesBySubdir := server.getImageFiles(mapping, attached)

	if len(files) != 2 {
		t.Fatalf("got %d files, want 2", len(files))
	}
	filesByName := map[string]map[string]interface{}{}
	for _, file := range files {
		filesByName[file["Name"].(string)] = file
	}
	if got := filesByName["system.hd1"]["DetectedType"]; got != "SCHD" {
		t.Errorf("detected type = %v, want SCHD", got)
	}
	if got := filesByName["system.hd1"]["DetectedTypeName"]; got != "SCSI Hard Disk" {
		t.Errorf("detected type name = %v, want SCSI Hard Disk", got)
	}
	if got := filesByName["system.hd1"]["InUse"]; got != false {
		t.Errorf("system.hd1 InUse = %v, want false", got)
	}
	if got := filesByName["nested/disc.toast"]["InUse"]; got != true {
		t.Errorf("nested/disc.toast InUse = %v, want true", got)
	}
	if _, ok := filesByName["notes.txt"]; ok {
		t.Error("unsupported notes.txt file was included")
	}
	if len(filesBySubdir[imageDir]) != 1 {
		t.Errorf("root file count = %d, want 1", len(filesBySubdir[imageDir]))
	}
	if len(filesBySubdir[nestedDir]) != 1 {
		t.Errorf("nested file count = %d, want 1", len(filesBySubdir[nestedDir]))
	}
}

func TestImageNameRelativeTo(t *testing.T) {
	baseDir := filepath.Join(string(filepath.Separator), "var", "lib", "piscsi", "images")
	tests := map[string]struct {
		name string
		want string
		ok   bool
	}{
		"absolute image": {name: filepath.Join(baseDir, "nested", "disk.hds"), want: "nested/disk.hds", ok: true},
		"relative image": {name: filepath.Join("nested", "disk.hds"), want: "nested/disk.hds", ok: true},
		"outside image":  {name: filepath.Join(baseDir, "..", "outside.hds"), ok: false},
		"empty image":    {name: "", ok: false},
	}

	for name, test := range tests {
		t.Run(name, func(t *testing.T) {
			got, ok := imageNameRelativeTo(baseDir, test.name)
			if got != test.want || ok != test.ok {
				t.Fatalf("imageNameRelativeTo() = (%q, %v), want (%q, %v)", got, ok, test.want, test.ok)
			}
		})
	}
}

func TestValidSCSIIDsRecommendsHighestAvailableID(t *testing.T) {
	valid, recommended := validSCSIIDs(
		map[int]struct{}{2: {}, 6: {}},
		[]int{7, 5},
	)
	want := []int{7, 5, 4, 3, 1, 0}
	if !reflect.DeepEqual(valid, want) {
		t.Fatalf("validSCSIIDs() = %v, want %v", valid, want)
	}
	if recommended != 4 {
		t.Fatalf("recommended ID = %d, want 4", recommended)
	}
}

func TestValidSCSIIDsFallsBackToFirstOccupiedID(t *testing.T) {
	valid, recommended := validSCSIIDs(nil, []int{3, 7, 1, 5, 0, 2, 4, 6})
	want := []int{7, 6, 5, 4, 3, 2, 1, 0}
	if !reflect.DeepEqual(valid, want) {
		t.Fatalf("validSCSIIDs() = %v, want %v", valid, want)
	}
	if recommended != 3 {
		t.Fatalf("recommended ID = %d, want 3", recommended)
	}
}

func TestFreeDiskSpaceMiB(t *testing.T) {
	free, err := freeDiskSpaceMiB(t.TempDir())
	if err != nil {
		t.Fatalf("freeDiskSpaceMiB() error = %v", err)
	}
	if free == 0 {
		t.Fatal("freeDiskSpaceMiB() returned zero for a writable temporary directory")
	}
}

func TestFormatFileSizeUsesUsefulUnits(t *testing.T) {
	tests := map[int64]string{
		0:               "0 bytes",
		1:               "1 byte",
		17:              "17 bytes",
		1536:            "1.5 KiB",
		2 * 1024 * 1024: "2.0 MiB",
	}
	for size, want := range tests {
		if got := formatFileSize(size); got != want {
			t.Errorf("formatFileSize(%d) = %q, want %q", size, got, want)
		}
	}
}

func TestGetImageFilesHandlesMissingDirectory(t *testing.T) {
	server := &Server{
		config: &config.Config{BaseDir: filepath.Join(t.TempDir(), "missing")},
	}

	files, filesBySubdir := server.getImageFiles(nil, nil)
	if len(files) != 0 || len(filesBySubdir) != 0 {
		t.Fatalf("got files %v grouped as %v, want empty results", files, filesBySubdir)
	}
}

func TestGetImageFilesIncludesMatchingPropertiesMetadata(t *testing.T) {
	imageDir := t.TempDir()
	configDir := t.TempDir()
	if err := os.WriteFile(filepath.Join(imageDir, "system.hds"), []byte("image"), 0o644); err != nil {
		t.Fatal(err)
	}
	properties := `{
		"vendor": "QUANTUM",
		"product": "FIREBALL",
		"revision": "1.0",
		"block_size": 512,
		"custom_note": "boot disk"
	}`
	if err := os.WriteFile(filepath.Join(configDir, "system.hds.properties"), []byte(properties), 0o644); err != nil {
		t.Fatal(err)
	}

	server := &Server{
		config: &config.Config{BaseDir: imageDir, ConfigDir: configDir},
		logger: slog.New(slog.NewTextHandler(io.Discard, nil)),
	}
	files, _ := server.getImageFiles(map[string]pb.PbDeviceType{"hds": pb.PbDeviceType_SCHD}, nil)
	if len(files) != 1 {
		t.Fatalf("got %d image files, want 1", len(files))
	}
	if got := files[0]["PropertiesFile"]; got != "system.hds.properties" {
		t.Fatalf("properties filename = %v, want system.hds.properties", got)
	}
	metadata, ok := files[0]["Properties"].([]propertyMetadata)
	if !ok {
		t.Fatalf("properties metadata has type %T", files[0]["Properties"])
	}
	gotNames := make([]string, 0, len(metadata))
	for _, item := range metadata {
		gotNames = append(gotNames, item.Name)
	}
	wantNames := []string{"vendor", "product", "revision", "block size", "custom note"}
	if !reflect.DeepEqual(gotNames, wantNames) {
		t.Fatalf("metadata names = %v, want %v", gotNames, wantNames)
	}
}

func TestImageAndPropertiesOperationsStaySynchronized(t *testing.T) {
	tests := []struct {
		name string
		run  func(imageDir, configDir string) error
	}{
		{
			name: "rename",
			run: func(imageDir, configDir string) error {
				oldImage, oldProperties, err := imageAndPropertiesPaths(imageDir, configDir, "old.hds")
				if err != nil {
					return err
				}
				newImage, newProperties, err := imageAndPropertiesPaths(imageDir, configDir, "new.hds")
				if err != nil {
					return err
				}
				withProperties, err := renameImageAndProperties(oldImage, newImage, oldProperties, newProperties)
				if err != nil {
					return err
				}
				if !withProperties {
					return os.ErrNotExist
				}
				for _, path := range []string{newImage, newProperties} {
					if _, err := os.Stat(path); err != nil {
						return err
					}
				}
				return nil
			},
		},
		{
			name: "copy",
			run: func(imageDir, configDir string) error {
				sourceImage, sourceProperties, err := imageAndPropertiesPaths(imageDir, configDir, "old.hds")
				if err != nil {
					return err
				}
				copyImage, copyProperties, err := imageAndPropertiesPaths(imageDir, configDir, "copy.hds")
				if err != nil {
					return err
				}
				withProperties, err := copyImageAndProperties(sourceImage, copyImage, sourceProperties, copyProperties)
				if err != nil {
					return err
				}
				if !withProperties {
					return os.ErrNotExist
				}
				for _, path := range []string{sourceImage, sourceProperties, copyImage, copyProperties} {
					if _, err := os.Stat(path); err != nil {
						return err
					}
				}
				return nil
			},
		},
		{
			name: "delete",
			run: func(imageDir, configDir string) error {
				image, properties, err := imageAndPropertiesPaths(imageDir, configDir, "old.hds")
				if err != nil {
					return err
				}
				withProperties, err := deleteImageAndProperties(image, properties)
				if err != nil {
					return err
				}
				if !withProperties {
					return os.ErrNotExist
				}
				for _, path := range []string{image, properties} {
					if _, err := os.Stat(path); !os.IsNotExist(err) {
						return fmt.Errorf("%s still exists (stat error: %v)", path, err)
					}
				}
				return nil
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			imageDir := t.TempDir()
			configDir := t.TempDir()
			if err := os.WriteFile(filepath.Join(imageDir, "old.hds"), []byte("image"), 0o640); err != nil {
				t.Fatal(err)
			}
			if err := os.WriteFile(filepath.Join(configDir, "old.hds.properties"), []byte(`{"vendor":"TEST"}`), 0o644); err != nil {
				t.Fatal(err)
			}
			if err := test.run(imageDir, configDir); err != nil {
				t.Fatal(err)
			}
		})
	}
}

func TestDrivePresetTemplateDataIncludesCDROMAndTape(t *testing.T) {
	tap := "tap"
	iso := "iso"
	size := int64(52445184)
	data := drivePresetTemplateData([]driveprops.DriveProperty{
		{DeviceType: "SCTP", Name: "Tape", FileType: &tap, Size: &size},
		{DeviceType: "SCCD", Name: "CD", FileType: &iso},
	})
	if len(data["CDROMDrives"]) != 1 {
		t.Fatalf("CDROMDrives count = %d, want 1", len(data["CDROMDrives"]))
	}
	if len(data["TapeDrives"]) != 1 {
		t.Fatalf("TapeDrives count = %d, want 1", len(data["TapeDrives"]))
	}
	if got := data["TapeDrives"][0]["FileType"]; got != "tap" {
		t.Fatalf("tape file type = %v, want tap", got)
	}
	if got := data["TapeDrives"][0]["SizeMB"]; got != "50.02" {
		t.Fatalf("tape size = %v, want 50.02", got)
	}
}

func TestCompatibleCDImagesUsesDaemonDetectedType(t *testing.T) {
	info := &pb.PbImageFilesInfo{ImageFiles: []*pb.PbImageFile{
		{Name: "custom.media", Type: pb.PbDeviceType_SCCD},
		{Name: "disk.iso", Type: pb.PbDeviceType_SCHD},
		{Name: "album.cue", Type: pb.PbDeviceType_SCCD},
	}}
	want := []string{"album.cue", "custom.media"}
	if got := compatibleCDImages(info); !reflect.DeepEqual(got, want) {
		t.Fatalf("compatibleCDImages() = %v, want %v", got, want)
	}
}

func TestHandleDriveCreateUsesConfigDirectoryAndExactPresetSize(t *testing.T) {
	gin.SetMode(gin.TestMode)
	root := t.TempDir()
	imageDir := filepath.Join(root, "images")
	configDir := filepath.Join(root, "config")
	for _, dir := range []string{imageDir, configDir} {
		if err := os.Mkdir(dir, 0o755); err != nil {
			t.Fatal(err)
		}
	}
	propertiesDatabase := filepath.Join(root, "drive_properties.json")
	const exactSize = int64(1048577)
	database := []map[string]interface{}{
		{
			"device_type": "SCTP",
			"vendor":      "TEST",
			"product":     "Tape",
			"revision":    nil,
			"block_size":  "",
			"size":        exactSize,
			"name":        "Test Tape",
			"file_type":   "tap",
		},
		{
			"device_type": "SCCD",
			"vendor":      "TEST",
			"product":     "CD-ROM",
			"revision":    "1.0",
			"block_size":  2048,
			"size":        nil,
			"name":        "Test CD",
			"file_type":   nil,
		},
	}
	data, err := json.Marshal(database)
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(propertiesDatabase, data, 0o644); err != nil {
		t.Fatal(err)
	}
	loaded, err := driveprops.LoadProperties(propertiesDatabase)
	if err != nil {
		t.Fatal(err)
	}

	server := &Server{
		config:       &config.Config{BaseDir: imageDir, ConfigDir: configDir},
		driveProps:   loaded,
		sessionStore: sessions.NewCookieStore([]byte("test-secret")),
	}
	form := url.Values{"file_name": {"backup"}, "drive_name": {"Test Tape"}}
	request := httptest.NewRequest(http.MethodPost, "/drive/create", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(response)
	context.Request = request

	server.handleDriveCreate(context)
	if response.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", response.Code, response.Body.String())
	}
	info, err := os.Stat(filepath.Join(imageDir, "backup.tap"))
	if err != nil {
		t.Fatal(err)
	}
	if info.Size() != exactSize {
		t.Fatalf("tape size = %d, want %d", info.Size(), exactSize)
	}
	if _, err := os.Stat(filepath.Join(configDir, "backup.tap.properties")); err != nil {
		t.Fatalf("properties were not created in config directory: %v", err)
	}
	if _, err := os.Stat(filepath.Join(imageDir, "backup.tap.properties")); !os.IsNotExist(err) {
		t.Fatalf("properties unexpectedly created beside image: %v", err)
	}

	if err := os.WriteFile(filepath.Join(imageDir, "install.iso"), []byte("iso"), 0o644); err != nil {
		t.Fatal(err)
	}
	cdForm := url.Values{"file_name": {"install.iso"}, "drive_name": {"Test CD"}}
	cdRequest := httptest.NewRequest(http.MethodPost, "/drive/cdrom", strings.NewReader(cdForm.Encode()))
	cdRequest.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	cdResponse := httptest.NewRecorder()
	cdContext, _ := gin.CreateTestContext(cdResponse)
	cdContext.Request = cdRequest
	server.handleDriveCdrom(cdContext)
	if cdResponse.Code != http.StatusSeeOther {
		t.Fatalf("CD-ROM status = %d, body = %s", cdResponse.Code, cdResponse.Body.String())
	}
	if _, err := os.Stat(filepath.Join(configDir, "install.iso.properties")); err != nil {
		t.Fatalf("CD-ROM properties were not created in config directory: %v", err)
	}
	if _, err := os.Stat(filepath.Join(imageDir, "install.iso.properties")); !os.IsNotExist(err) {
		t.Fatalf("CD-ROM properties unexpectedly created beside image: %v", err)
	}
}

func TestHandleFilesCreateCanWriteSelectedDriveProperties(t *testing.T) {
	gin.SetMode(gin.TestMode)
	root := t.TempDir()
	imageDir := filepath.Join(root, "images")
	configDir := filepath.Join(root, "config")
	for _, dir := range []string{imageDir, configDir} {
		if err := os.Mkdir(dir, 0o755); err != nil {
			t.Fatal(err)
		}
	}
	databasePath := filepath.Join(root, "drives.json")
	database := `[{
		"device_type":"SCHD",
		"vendor":"TEST",
		"product":"Disk",
		"revision":"1.0",
		"block_size":512,
		"size":1048576,
		"name":"Test Disk",
		"file_type":"hds"
	}]`
	if err := os.WriteFile(databasePath, []byte(database), 0o644); err != nil {
		t.Fatal(err)
	}
	properties, err := driveprops.LoadProperties(databasePath)
	if err != nil {
		t.Fatal(err)
	}

	server := newImageCreationTestServer(imageDir, configDir, properties)
	form := url.Values{
		"file_name":  {"custom"},
		"size":       {"2"},
		"type":       {"hds"},
		"drive_name": {"Test Disk"},
	}
	response := performImageCreateRequest(server, form)
	if response.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", response.Code, response.Body.String())
	}
	imageInfo, err := os.Stat(filepath.Join(imageDir, "custom.hds"))
	if err != nil {
		t.Fatal(err)
	}
	if imageInfo.Size() != 2*1024*1024 {
		t.Fatalf("image size = %d, want %d", imageInfo.Size(), 2*1024*1024)
	}
	propertiesData, err := os.ReadFile(filepath.Join(configDir, "custom.hds.properties"))
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(propertiesData), `"name": "Test Disk"`) {
		t.Fatalf("properties do not contain selected profile: %s", propertiesData)
	}
}

func TestHandleFilesCreateRejectsFormatBeforeCreatingImage(t *testing.T) {
	gin.SetMode(gin.TestMode)
	imageDir := t.TempDir()
	configDir := t.TempDir()
	server := newImageCreationTestServer(imageDir, configDir, nil)
	form := url.Values{
		"file_name":    {"invalid"},
		"size":         {"1"},
		"type":         {"hds"},
		"drive_format": {"ext4"},
	}
	response := performImageCreateRequest(server, form)
	if response.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", response.Code, response.Body.String())
	}
	if _, err := os.Stat(filepath.Join(imageDir, "invalid.hds")); !os.IsNotExist(err) {
		t.Fatalf("invalid format left an image behind: %v", err)
	}
}

func TestHandleFilesCreateRemovesImageWhenFormattingFails(t *testing.T) {
	gin.SetMode(gin.TestMode)
	root := t.TempDir()
	binDir := filepath.Join(root, "bin")
	if err := os.Mkdir(binDir, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(binDir, "fdisk"), []byte("#!/bin/sh\nexit 1\n"), 0o755); err != nil {
		t.Fatal(err)
	}
	t.Setenv("PATH", binDir+string(os.PathListSeparator)+os.Getenv("PATH"))

	imageDir := filepath.Join(root, "images")
	configDir := filepath.Join(root, "config")
	for _, dir := range []string{imageDir, configDir} {
		if err := os.Mkdir(dir, 0o755); err != nil {
			t.Fatal(err)
		}
	}
	server := newImageCreationTestServer(imageDir, configDir, nil)
	form := url.Values{
		"file_name":    {"format-failure"},
		"size":         {"1"},
		"type":         {"hds"},
		"drive_format": {"FAT16"},
	}
	response := performImageCreateRequest(server, form)
	if response.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", response.Code, response.Body.String())
	}
	if _, err := os.Stat(filepath.Join(imageDir, "format-failure.hds")); !os.IsNotExist(err) {
		t.Fatalf("formatting failure left an image behind: %v", err)
	}
}

func TestFormatNewImageInjectsHFSDriver(t *testing.T) {
	root := t.TempDir()
	binDir := filepath.Join(root, "bin")
	driverDir := filepath.Join(root, "drivers")
	for _, dir := range []string{binDir, driverDir} {
		if err := os.Mkdir(dir, 0o755); err != nil {
			t.Fatal(err)
		}
	}
	if err := os.WriteFile(filepath.Join(binDir, "hfdisk"), []byte("#!/bin/sh\nexit 0\n"), 0o755); err != nil {
		t.Fatal(err)
	}
	// hformat always opens $HOME/.hcwd. Make the inherited HOME unusable
	// and verify that the formatter supplies its own writable home.
	if err := os.WriteFile(filepath.Join(binDir, "hformat"), []byte("#!/bin/sh\n: > \"$HOME/.hcwd\"\n"), 0o755); err != nil {
		t.Fatal(err)
	}
	unusableHome := filepath.Join(root, "unusable-home")
	if err := os.WriteFile(unusableHome, []byte("not a directory"), 0o644); err != nil {
		t.Fatal(err)
	}
	t.Setenv("HOME", unusableHome)
	t.Setenv("PATH", binDir+string(os.PathListSeparator)+os.Getenv("PATH"))

	driverData := make([]byte, 32*512)
	for i := range driverData {
		driverData[i] = byte(i)
	}
	driverPath := filepath.Join(driverDir, "Lido-7.56.img")
	if err := os.WriteFile(driverPath, driverData, 0o644); err != nil {
		t.Fatal(err)
	}
	imagePath := filepath.Join(root, "disk.hds")
	if err := os.WriteFile(imagePath, make([]byte, 1024*1024), 0o644); err != nil {
		t.Fatal(err)
	}

	server := &Server{config: &config.Config{DriverDir: driverDir}}
	if err := server.formatNewImage(imagePath, 1, "Lido 7.56"); err != nil {
		t.Fatalf("formatNewImage() error = %v", err)
	}
	imageData, err := os.ReadFile(imagePath)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(imageData[64*512:96*512], driverData) {
		t.Fatal("HFS driver was not injected at blocks 64-95")
	}
}

func TestFormatNewImageSupportsFAT16AndFAT32(t *testing.T) {
	root := t.TempDir()
	binDir := filepath.Join(root, "bin")
	if err := os.Mkdir(binDir, 0o755); err != nil {
		t.Fatal(err)
	}
	scripts := map[string]string{
		"fdisk":    "#!/bin/sh\ncat >/dev/null\nexit 0\n",
		"mkfs.fat": "#!/bin/sh\nexit 0\n",
	}
	for name, contents := range scripts {
		if err := os.WriteFile(filepath.Join(binDir, name), []byte(contents), 0o755); err != nil {
			t.Fatal(err)
		}
	}
	t.Setenv("PATH", binDir+string(os.PathListSeparator)+os.Getenv("PATH"))

	server := &Server{config: &config.Config{}}
	for _, format := range []string{"FAT16", "FAT32"} {
		t.Run(format, func(t *testing.T) {
			imagePath := filepath.Join(root, format+".hds")
			if err := os.WriteFile(imagePath, make([]byte, 1024*1024), 0o644); err != nil {
				t.Fatal(err)
			}
			if err := server.formatNewImage(imagePath, 1, format); err != nil {
				t.Fatalf("formatNewImage() error = %v", err)
			}
		})
	}
}

func newImageCreationTestServer(imageDir, configDir string, properties *driveprops.Properties) *Server {
	return &Server{
		config:       &config.Config{BaseDir: imageDir, ConfigDir: configDir},
		driveProps:   properties,
		sessionStore: sessions.NewCookieStore([]byte("test-secret")),
		piscsiClient: &testutil.MockPiSCSIClient{
			SendCommandFunc: func(command *pb.PbCommand) (*pb.PbResult, error) {
				return &pb.PbResult{
					Status: true,
					Result: &pb.PbResult_ServerInfo{
						ServerInfo: &pb.PbServerInfo{
							MappingInfo: &pb.PbMappingInfo{Mapping: map[string]pb.PbDeviceType{
								"hds": pb.PbDeviceType_SCHD,
								"hdr": pb.PbDeviceType_SCRM,
								"mos": pb.PbDeviceType_SCMO,
								"tap": pb.PbDeviceType_SCTP,
								"hdi": pb.PbDeviceType_SCHD,
							}},
						},
					},
				}, nil
			},
		},
	}
}

func performImageCreateRequest(server *Server, form url.Values) *httptest.ResponseRecorder {
	request := httptest.NewRequest(http.MethodPost, "/files/create", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(response)
	context.Request = request
	server.handleFilesCreate(context)
	return response
}
