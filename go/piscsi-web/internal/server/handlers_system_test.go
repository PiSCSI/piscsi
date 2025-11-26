package server

import (
	"errors"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi-web/internal/server/testutil"
	pb "github.com/piscsi/piscsi-web/proto"
)

func TestHandleHostPowerOperationsUsePiSCSI(t *testing.T) {
	tests := []struct {
		name     string
		endpoint string
		mode     string
	}{
		{name: "shutdown", endpoint: "/sys/shutdown", mode: "system"},
		{name: "reboot", endpoint: "/sys/reboot", mode: "reboot"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
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

			router := gin.New()
			if tt.mode == "system" {
				router.POST(tt.endpoint, server.handleSysShutdown)
			} else {
				router.POST(tt.endpoint, server.handleSysReboot)
			}
			request := httptest.NewRequest(http.MethodPost, tt.endpoint, nil)
			response := httptest.NewRecorder()
			router.ServeHTTP(response, request)

			if response.Code != http.StatusSeeOther {
				t.Fatalf("status = %d, want %d", response.Code, http.StatusSeeOther)
			}
			if command == nil {
				t.Fatal("PiSCSI command was not sent")
			}
			if command.GetOperation() != pb.PbOperation_SHUT_DOWN {
				t.Errorf("operation = %s, want SHUT_DOWN", command.GetOperation())
			}
			if got := command.GetParams()["mode"]; got != tt.mode {
				t.Errorf("mode = %q, want %q", got, tt.mode)
			}
		})
	}
}

func TestHandleHostPowerOperationReportsDaemonErrors(t *testing.T) {
	tests := []struct {
		name     string
		client   *testutil.MockPiSCSIClient
		wantText string
	}{
		{
			name:     "communication error",
			client:   testutil.NewMockPiSCSIClientWithError(errors.New("connection refused")),
			wantText: "Failed to communicate with piscsi daemon",
		},
		{
			name:     "rejected",
			client:   testutil.NewMockPiSCSIClientAlwaysFail("permission denied"),
			wantText: "permission denied",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			gin.SetMode(gin.TestMode)
			server := &Server{
				sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
				piscsiClient: tt.client,
				logger:       slog.New(slog.NewTextHandler(io.Discard, nil)),
			}
			router := gin.New()
			router.POST("/sys/shutdown", server.handleSysShutdown)
			request := httptest.NewRequest(http.MethodPost, "/sys/shutdown", nil)
			response := httptest.NewRecorder()
			router.ServeHTTP(response, request)

			if response.Code != http.StatusSeeOther {
				t.Fatalf("status = %d, want %d", response.Code, http.StatusSeeOther)
			}
			session := testutil.GetSessionFromResponse(t, response.Result(), server.sessionStore, request)
			_, message := GetFlashesForTemplate(session)
			if !strings.Contains(message, tt.wantText) {
				t.Errorf("message = %q, want text %q", message, tt.wantText)
			}
		})
	}
}
