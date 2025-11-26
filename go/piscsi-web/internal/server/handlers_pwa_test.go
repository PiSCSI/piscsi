package server

import (
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi-web/internal/config"
)

func TestPWAResourcesArePublic(t *testing.T) {
	gin.SetMode(gin.TestMode)

	server := &Server{
		config:       &config.Config{StaticDir: t.TempDir()},
		router:       gin.New(),
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
	}
	server.setupRoutes()

	tests := []struct {
		path        string
		contentType string
	}{
		{path: "/pwa/manifest.json", contentType: "application/json"},
		{path: "/pwa/favicon-32x32.png", contentType: "image/png"},
		{path: "/pwa/favicon.ico", contentType: "image/x-icon"},
		{path: "/pwa/browserconfig.xml", contentType: "application/xml"},
	}

	for _, test := range tests {
		t.Run(test.path, func(t *testing.T) {
			recorder := httptest.NewRecorder()
			request := httptest.NewRequest(http.MethodGet, test.path, nil)

			server.router.ServeHTTP(recorder, request)

			if recorder.Code != http.StatusOK {
				t.Fatalf("status = %d, want %d", recorder.Code, http.StatusOK)
			}
			if location := recorder.Header().Get("Location"); location != "" {
				t.Errorf("Location = %q, want no login redirect", location)
			}
			if contentType := recorder.Header().Get("Content-Type"); !strings.HasPrefix(contentType, test.contentType) {
				t.Errorf("Content-Type = %q, want prefix %q", contentType, test.contentType)
			}
		})
	}
}

func TestPWAMissingResourceReturnsNotFound(t *testing.T) {
	gin.SetMode(gin.TestMode)

	server := &Server{
		config: &config.Config{StaticDir: t.TempDir()},
		router: gin.New(),
	}
	server.setupRoutes()

	recorder := httptest.NewRecorder()
	request := httptest.NewRequest(http.MethodGet, "/pwa/missing.png", nil)
	server.router.ServeHTTP(recorder, request)

	if recorder.Code != http.StatusNotFound {
		t.Fatalf("status = %d, want %d", recorder.Code, http.StatusNotFound)
	}
}

func TestPWARejectsTraversalBeforeFilesystemFallback(t *testing.T) {
	gin.SetMode(gin.TestMode)

	staticDir := t.TempDir()
	secretPath := filepath.Join(staticDir, "secret.txt")
	if err := os.WriteFile(secretPath, []byte("must not be served"), 0o600); err != nil {
		t.Fatalf("write traversal target: %v", err)
	}

	server := &Server{config: &config.Config{StaticDir: staticDir}}
	recorder := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(recorder)
	context.Params = []gin.Param{{Key: "pwa_path", Value: "/../secret.txt"}}
	context.Request = httptest.NewRequest(http.MethodGet, "/pwa/../secret.txt", nil)

	server.handlePWA(context)

	if recorder.Code != http.StatusNotFound {
		t.Fatalf("status = %d, want %d", recorder.Code, http.StatusNotFound)
	}
	if strings.Contains(recorder.Body.String(), "must not be served") {
		t.Fatal("filesystem fallback served a file outside the PWA directory")
	}
}
