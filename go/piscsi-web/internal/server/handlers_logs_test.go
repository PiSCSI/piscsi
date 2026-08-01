package server

import (
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/server/testutil"
	pb "github.com/piscsi/piscsi/go/proto"
)

func TestHandleLogsLevelUsesDaemonLogLevelName(t *testing.T) {
	gin.SetMode(gin.TestMode)

	var command *pb.PbCommand
	client := &testutil.MockPiSCSIClient{
		SendCommandFunc: func(cmd *pb.PbCommand) (*pb.PbResult, error) {
			command = cmd
			return &pb.PbResult{Status: true}, nil
		},
	}
	server := &Server{
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
		piscsiClient: client,
		logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	form := url.Values{"level": {"warning"}}
	request := httptest.NewRequest(http.MethodPost, "/logs/level", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()
	router := gin.New()
	router.POST("/logs/level", server.handleLogsLevel)
	router.ServeHTTP(response, request)

	if response.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, want %d", response.Code, http.StatusSeeOther)
	}
	if command == nil {
		t.Fatal("PiSCSI command was not sent")
	}
	if got := command.GetParams()["level"]; got != "warning" {
		t.Errorf("log level = %q, want %q", got, "warning")
	}
}
