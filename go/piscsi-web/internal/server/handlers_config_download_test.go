package server

import (
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/config"
)

func TestConfigurationDownloadsRejectSymlinks(t *testing.T) {
	gin.SetMode(gin.TestMode)
	configDir := t.TempDir()
	secretPath := filepath.Join(t.TempDir(), "secret.json")
	if err := os.WriteFile(secretPath, []byte("secret"), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := os.Symlink(secretPath, filepath.Join(configDir, "download.json")); err != nil {
		t.Skipf("create symbolic link: %v", err)
	}

	server := &Server{config: &config.Config{ConfigDir: configDir}}
	configDownloadRequest := httptest.NewRequest(http.MethodPost, "/files/download_config",
		strings.NewReader(url.Values{"file": {"download.json"}}.Encode()))
	configDownloadRequest.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	tests := []struct {
		name    string
		request *http.Request
		handler func(*gin.Context)
	}{
		{
			name:    "configuration download endpoint",
			request: configDownloadRequest,
			handler: server.handleFilesDownloadConfig,
		},
		{
			name:    "generic download endpoint with config source",
			request: httptest.NewRequest(http.MethodGet, "/files/download_image?source=config&file=download.json", nil),
			handler: server.handleFilesDownload,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			response := httptest.NewRecorder()
			context, _ := gin.CreateTestContext(response)
			context.Request = test.request
			test.handler(context)
			if response.Code != http.StatusBadRequest {
				t.Fatalf("status = %d, want %d; body = %s", response.Code, http.StatusBadRequest, response.Body.String())
			}
			if strings.Contains(response.Body.String(), "secret") {
				t.Fatal("download response exposed symlink target content")
			}
		})
	}
}
