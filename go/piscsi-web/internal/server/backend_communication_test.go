package server

import (
	"errors"
	"io"
	"log/slog"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi-web/internal/config"
	"github.com/piscsi/piscsi-web/internal/server/testutil"
	pb "github.com/piscsi/piscsi-web/proto"
)

func TestCommandBuilderUsesConfiguredTokenAndSessionLocale(t *testing.T) {
	gin.SetMode(gin.TestMode)
	store := sessions.NewCookieStore([]byte("test-secret-key"))

	sessionRequest := httptest.NewRequest("GET", "/", nil)
	sessionResponse := httptest.NewRecorder()
	session, err := store.Get(sessionRequest, sessionName)
	if err != nil {
		t.Fatal(err)
	}
	session.Values["language"] = "de"
	if err := session.Save(sessionRequest, sessionResponse); err != nil {
		t.Fatal(err)
	}

	request := httptest.NewRequest("GET", "/", nil)
	for _, cookie := range sessionResponse.Result().Cookies() {
		request.AddCookie(cookie)
	}
	contextResponse := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(contextResponse)
	context.Request = request

	server := &Server{
		config:       &config.Config{PiscsiToken: "daemon-secret"},
		sessionStore: store,
	}
	command := server.getCommandBuilder(context).ServerInfo()
	if got := command.GetParams()["token"]; got != "daemon-secret" {
		t.Errorf("token = %q, want configured token", got)
	}
	if got := command.GetParams()["locale"]; got != "de" {
		t.Errorf("locale = %q, want session locale", got)
	}
}

func TestCheckBackendAuthentication(t *testing.T) {
	tests := []struct {
		name       string
		token      string
		result     *pb.PbResult
		clientErr  error
		wantError  string
		wantToken  string
		wantLocale string
	}{
		{
			name:       "unprotected or valid token",
			token:      "daemon-secret",
			result:     &pb.PbResult{Status: true},
			wantToken:  "daemon-secret",
			wantLocale: "en",
		},
		{
			name:       "missing token",
			result:     &pb.PbResult{Status: false, ErrorCode: pb.PbErrorCode_UNAUTHORIZED},
			wantError:  "configure --password or PISCSI_TOKEN",
			wantLocale: "en",
		},
		{
			name:       "invalid token",
			token:      "wrong",
			result:     &pb.PbResult{Status: false, Msg: "Authentication failed", ErrorCode: pb.PbErrorCode_UNAUTHORIZED},
			wantError:  "is invalid",
			wantToken:  "wrong",
			wantLocale: "en",
		},
		{
			name:       "communication failure",
			clientErr:  errors.New("connection refused"),
			wantError:  "check PiSCSI daemon authentication",
			wantLocale: "en",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			var command *pb.PbCommand
			server := &Server{
				config: &config.Config{PiscsiToken: test.token},
				piscsiClient: &testutil.MockPiSCSIClient{
					SendCommandFunc: func(cmd *pb.PbCommand) (*pb.PbResult, error) {
						command = cmd
						return test.result, test.clientErr
					},
				},
				logger: slog.New(slog.NewTextHandler(io.Discard, nil)),
			}

			err := server.checkBackendAuthentication()
			if test.wantError == "" {
				if err != nil {
					t.Fatalf("checkBackendAuthentication() error = %v", err)
				}
			} else if err == nil || !strings.Contains(err.Error(), test.wantError) {
				t.Fatalf("checkBackendAuthentication() error = %v, want text %q", err, test.wantError)
			}
			if command == nil || command.GetOperation() != pb.PbOperation_CHECK_AUTHENTICATION {
				t.Fatalf("command = %v, want CHECK_AUTHENTICATION", command)
			}
			if got := command.GetParams()["token"]; got != test.wantToken {
				t.Errorf("token = %q, want %q", got, test.wantToken)
			}
			if got := command.GetParams()["locale"]; got != test.wantLocale {
				t.Errorf("locale = %q, want %q", got, test.wantLocale)
			}
		})
	}
}
