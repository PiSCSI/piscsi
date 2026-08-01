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
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/config"
	"github.com/piscsi/piscsi/go/piscsi-web/web"
	xhtml "golang.org/x/net/html"
)

func TestHandleFilesDiskinfoDisplaysStructuredDisktypeOutput(t *testing.T) {
	gin.SetMode(gin.TestMode)

	imageDir := t.TempDir()
	imagePath := filepath.Join(imageDir, "hfs.hda")
	if err := os.WriteFile(imagePath, []byte("disk image"), 0o600); err != nil {
		t.Fatal(err)
	}

	const diskInfo = `Apple partition map
  Partition 1
    Type "Apple_partition_map"
  Partition 2
    Type "Apple_HFS"
`
	server := &Server{
		config:       &config.Config{BaseDir: imageDir},
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
		hostname:     func() string { return "piscsi" },
		hostIP:       func() string { return "127.0.0.1" },
		systemCommand: func(name string, args ...string) ([]byte, error) {
			switch name {
			case "disktype":
				if len(args) != 1 || args[0] != imagePath {
					t.Fatalf("disktype arguments = %q, want [%q]", args, imagePath)
				}
				return []byte(diskInfo), nil
			case "vcgencmd":
				return []byte("not supported"), nil
			default:
				t.Fatalf("unexpected command %q", name)
				return nil, nil
			}
		},
	}

	templates, err := web.GetTemplates()
	if err != nil {
		t.Fatal(err)
	}
	router := gin.New()
	router.SetHTMLTemplate(templates)
	router.POST("/files/diskinfo", server.handleFilesDiskinfo)

	form := url.Values{"file_name": {"hfs.hda"}}
	request := httptest.NewRequest(http.MethodPost, "/files/diskinfo", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()
	router.ServeHTTP(response, request)

	if response.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", response.Code, http.StatusOK)
	}

	document, err := xhtml.Parse(response.Body)
	if err != nil {
		t.Fatalf("parse response: %v", err)
	}
	sample := findHTMLElement(document, "samp")
	if sample == nil || sample.FirstChild == nil {
		t.Fatal("response does not contain disk info output")
	}
	if got := sample.FirstChild.Data; got != diskInfo {
		t.Errorf("disk info output = %q, want %q", got, diskInfo)
	}
}

func findHTMLElement(node *xhtml.Node, name string) *xhtml.Node {
	if node.Type == xhtml.ElementNode && node.Data == name {
		return node
	}
	for child := node.FirstChild; child != nil; child = child.NextSibling {
		if match := findHTMLElement(child, name); match != nil {
			return match
		}
	}
	return nil
}
