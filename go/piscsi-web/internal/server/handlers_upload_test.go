package server

import (
	"bytes"
	"io"
	"log/slog"
	"mime/multipart"
	"net/http"
	"net/http/httptest"
	neturl "net/url"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi-web/internal/config"
)

type uploadFormField struct {
	name  string
	value string
}

func newUploadTestServer(t *testing.T, maxFileSize int64) (*Server, string) {
	t.Helper()

	root := t.TempDir()
	imageDir := filepath.Join(root, "images")
	if err := os.Mkdir(imageDir, 0755); err != nil {
		t.Fatalf("create image directory: %v", err)
	}

	return &Server{
		config: &config.Config{
			BaseDir:     imageDir,
			SharedDir:   filepath.Join(root, "shared"),
			ConfigDir:   filepath.Join(root, "config"),
			MaxFileSize: maxFileSize,
		},
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
	}, imageDir
}

func performUpload(
	t *testing.T,
	server *Server,
	fields []uploadFormField,
	filename string,
	content []byte,
) *httptest.ResponseRecorder {
	t.Helper()

	var body bytes.Buffer
	writer := multipart.NewWriter(&body)
	for _, field := range fields {
		if err := writer.WriteField(field.name, field.value); err != nil {
			t.Fatalf("write form field %q: %v", field.name, err)
		}
	}
	if filename != "" {
		filePart, err := writer.CreateFormFile("file", filename)
		if err != nil {
			t.Fatalf("create file part: %v", err)
		}
		if _, err := filePart.Write(content); err != nil {
			t.Fatalf("write file part: %v", err)
		}
	}
	if err := writer.Close(); err != nil {
		t.Fatalf("close multipart writer: %v", err)
	}

	recorder := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(recorder)
	context.Request = httptest.NewRequest(http.MethodPost, "/files/upload", &body)
	context.Request.Header.Set("Content-Type", writer.FormDataContentType())

	server.handleFilesUpload(context)
	return recorder
}

func assertNoTemporaryUploads(t *testing.T, directory string) {
	t.Helper()

	matches, err := filepath.Glob(filepath.Join(directory, ".piscsi-upload-*"))
	if err != nil {
		t.Fatalf("find temporary uploads: %v", err)
	}
	if len(matches) != 0 {
		t.Fatalf("temporary upload files remain: %v", matches)
	}
}

func TestHandleFilesUploadStreamsFile(t *testing.T) {
	gin.SetMode(gin.TestMode)
	content := bytes.Repeat([]byte("streamed upload data\n"), 8192)
	server, imageDir := newUploadTestServer(t, int64(len(content)))

	recorder := performUpload(t, server, []uploadFormField{
		{name: "destination", value: "disk_images"},
		{name: "images_subdir", value: ""},
	}, "disk.hda", content)

	if recorder.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", recorder.Code, recorder.Body.String())
	}
	uploaded, err := os.ReadFile(filepath.Join(imageDir, "disk.hda"))
	if err != nil {
		t.Fatalf("read uploaded file: %v", err)
	}
	if !bytes.Equal(uploaded, content) {
		t.Fatal("uploaded file content does not match")
	}

	assertNoTemporaryUploads(t, imageDir)
}

func TestHandleFilesUploadRejectsOversizedFile(t *testing.T) {
	gin.SetMode(gin.TestMode)
	server, imageDir := newUploadTestServer(t, 4)

	recorder := performUpload(t, server, []uploadFormField{
		{name: "destination", value: "disk_images"},
	}, "too-large.hda", []byte("12345"))

	if recorder.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", recorder.Code, recorder.Body.String())
	}
	if _, err := os.Stat(filepath.Join(imageDir, "too-large.hda")); !os.IsNotExist(err) {
		t.Fatalf("oversized destination exists or stat failed: %v", err)
	}
	assertNoTemporaryUploads(t, imageDir)
}

func TestHandleFilesUploadDoesNotOverwriteExistingFile(t *testing.T) {
	gin.SetMode(gin.TestMode)
	server, imageDir := newUploadTestServer(t, 1024)
	existingPath := filepath.Join(imageDir, "existing.hda")
	if err := os.WriteFile(existingPath, []byte("original"), 0644); err != nil {
		t.Fatalf("create existing file: %v", err)
	}

	recorder := performUpload(t, server, []uploadFormField{
		{name: "destination", value: "disk_images"},
	}, "existing.hda", []byte("replacement"))

	if recorder.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", recorder.Code, recorder.Body.String())
	}
	content, err := os.ReadFile(existingPath)
	if err != nil {
		t.Fatalf("read existing file: %v", err)
	}
	if string(content) != "original" {
		t.Fatalf("existing content = %q, want original", content)
	}
	assertNoTemporaryUploads(t, imageDir)
}

func TestHandleFilesUploadRejectsEscapingSubdirectory(t *testing.T) {
	gin.SetMode(gin.TestMode)
	server, imageDir := newUploadTestServer(t, 1024)

	recorder := performUpload(t, server, []uploadFormField{
		{name: "destination", value: "disk_images"},
		{name: "images_subdir", value: "../outside"},
	}, "escape.hda", []byte("data"))

	if recorder.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", recorder.Code, recorder.Body.String())
	}
	if _, err := os.Stat(filepath.Join(filepath.Dir(imageDir), "outside", "escape.hda")); !os.IsNotExist(err) {
		t.Fatalf("escaped destination exists or stat failed: %v", err)
	}
	assertNoTemporaryUploads(t, imageDir)
}

func performURLDownload(t *testing.T, server *Server, values neturl.Values) *httptest.ResponseRecorder {
	t.Helper()

	recorder := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(recorder)
	context.Request = httptest.NewRequest(
		http.MethodPost,
		"/files/download_url",
		strings.NewReader(values.Encode()),
	)
	context.Request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	server.handleFilesDownloadURL(context)
	return recorder
}

func TestHandleFilesDownloadURLUsesSelectedSubdirectories(t *testing.T) {
	gin.SetMode(gin.TestMode)
	server, imageDir := newUploadTestServer(t, 1024)
	imageSubdir := filepath.Join(imageDir, "nested", "images")
	sharedSubdir := filepath.Join(server.config.SharedDir, "nested", "shared")
	for _, directory := range []string{imageSubdir, sharedSubdir} {
		if err := os.MkdirAll(directory, 0755); err != nil {
			t.Fatalf("create destination directory: %v", err)
		}
	}

	content := []byte("downloaded data")
	source := httptest.NewServer(http.HandlerFunc(func(response http.ResponseWriter, _ *http.Request) {
		_, _ = response.Write(content)
	}))
	defer source.Close()

	tests := []struct {
		name        string
		destination string
		field       string
		subdir      string
		wantDir     string
	}{
		{
			name:        "images",
			destination: "disk_images",
			field:       "images_subdir",
			subdir:      "nested/images",
			wantDir:     imageSubdir,
		},
		{
			name:        "shared",
			destination: "shared_files",
			field:       "shared_subdir",
			subdir:      "nested/shared",
			wantDir:     sharedSubdir,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			values := neturl.Values{
				"url":         {source.URL + "/disk%20image.hds?download=1"},
				"destination": {test.destination},
				test.field:    {test.subdir},
			}
			recorder := performURLDownload(t, server, values)
			if recorder.Code != http.StatusSeeOther {
				t.Fatalf("status = %d, body = %s", recorder.Code, recorder.Body.String())
			}

			got, err := os.ReadFile(filepath.Join(test.wantDir, "disk image.hds"))
			if err != nil {
				t.Fatalf("read downloaded file: %v", err)
			}
			if !bytes.Equal(got, content) {
				t.Fatalf("downloaded content = %q, want %q", got, content)
			}
		})
	}
}

func TestHandleFilesDownloadURLRejectsEscapingSubdirectory(t *testing.T) {
	gin.SetMode(gin.TestMode)
	server, imageDir := newUploadTestServer(t, 1024)

	recorder := performURLDownload(t, server, neturl.Values{
		"url":           {"https://example.invalid/disk.hds"},
		"destination":   {"disk_images"},
		"images_subdir": {"../outside"},
	})

	if recorder.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, body = %s", recorder.Code, recorder.Body.String())
	}
	if _, err := os.Stat(filepath.Join(filepath.Dir(imageDir), "outside", "disk.hds")); !os.IsNotExist(err) {
		t.Fatalf("escaped destination exists or stat failed: %v", err)
	}
}

func TestAddTransferDirectoryDataUsesConfiguredSharedDirectory(t *testing.T) {
	server, imageDir := newUploadTestServer(t, 1024)
	for _, directory := range []string{
		filepath.Join(imageDir, "z"),
		filepath.Join(imageDir, "a", "nested"),
		filepath.Join(imageDir, ".hidden", "ignored"),
		filepath.Join(server.config.SharedDir, "shared", "nested"),
	} {
		if err := os.MkdirAll(directory, 0755); err != nil {
			t.Fatalf("create transfer directory: %v", err)
		}
	}

	data := gin.H{}
	server.addTransferDirectoryData(data)

	if data["SharedDir"] != server.config.SharedDir {
		t.Fatalf("SharedDir = %v, want %q", data["SharedDir"], server.config.SharedDir)
	}
	if got, want := data["ImagesSubdirs"], []string{"a", filepath.Join("a", "nested"), "z"}; !slicesEqual(got, want) {
		t.Fatalf("ImagesSubdirs = %v, want %v", got, want)
	}
	if got, want := data["SharedSubdirs"], []string{"shared", filepath.Join("shared", "nested")}; !slicesEqual(got, want) {
		t.Fatalf("SharedSubdirs = %v, want %v", got, want)
	}
}

func slicesEqual(got any, want []string) bool {
	values, ok := got.([]string)
	if !ok || len(values) != len(want) {
		return false
	}
	for index := range values {
		if values[index] != want[index] {
			return false
		}
	}
	return true
}
