package server

import (
	"errors"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi/go/piscsi-web/web"
)

func TestCompanionServicesUsesSystemd(t *testing.T) {
	active := map[string]bool{
		"netatalk": true,
		"smbd":     true,
		"macproxy": true,
		"webmin":   true,
	}
	var calls []string
	server := &Server{
		systemCommand: func(name string, args ...string) ([]byte, error) {
			if name != "systemctl" || len(args) != 3 || args[0] != "is-active" || args[1] != "--quiet" {
				t.Fatalf("command = %q %q, want systemctl is-active --quiet SERVICE", name, args)
			}
			calls = append(calls, args[2])
			if active[args[2]] {
				return nil, nil
			}
			return nil, errors.New("exit status 3")
		},
	}

	got := server.companionServices()
	want := serviceStatus{Netatalk: true, Samba: true, Macproxy: true, Webmin: true}
	if got != want {
		t.Fatalf("companionServices() = %+v, want %+v", got, want)
	}
	if got, want := strings.Join(calls, ","), "netatalk,smbd,vsftpd,macproxy,webmin"; got != want {
		t.Errorf("checked services = %q, want %q", got, want)
	}
}

func TestParseThrottleNotices(t *testing.T) {
	got := parseThrottleNotices("throttled=0x10001\n")
	if len(got) != 2 {
		t.Fatalf("len(parseThrottleNotices()) = %d, want 2", len(got))
	}
	if got[0].Category != "error" || got[0].ReturnCode != 100 {
		t.Errorf("current undervoltage notice = %+v", got[0])
	}
	if got[1].Category != "warning" || got[1].ReturnCode != 116 {
		t.Errorf("historic undervoltage notice = %+v", got[1])
	}
	if got := parseThrottleNotices("not supported"); len(got) != 0 {
		t.Errorf("invalid throttle output returned %+v", got)
	}
}

func TestHandleLogsShowReturnsErrorWhenJournalctlFails(t *testing.T) {
	gin.SetMode(gin.TestMode)
	server := &Server{
		systemCommand: func(name string, _ ...string) ([]byte, error) {
			switch name {
			case "journalctl":
				return []byte("access denied"), errors.New("exit status 1")
			case "vcgencmd":
				return []byte("not supported"), nil
			default:
				t.Fatalf("unexpected command %q", name)
				return nil, nil
			}
		},
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
	}
	templates, err := web.GetTemplates()
	if err != nil {
		t.Fatal(err)
	}
	router := gin.New()
	router.SetHTMLTemplate(templates)
	router.POST("/logs/show", server.handleLogsShow)
	request := httptest.NewRequest(http.MethodPost, "/logs/show",
		strings.NewReader("lines=100&scope=piscsi"))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()
	router.ServeHTTP(response, request)

	if response.Code != http.StatusInternalServerError {
		t.Fatalf("status = %d, want %d", response.Code, http.StatusInternalServerError)
	}
	if !strings.Contains(response.Body.String(), "access denied") {
		t.Errorf("body = %q, want journal error details", response.Body.String())
	}
}

func TestHandleLogsShowValidatesAndBoundsRequestParameters(t *testing.T) {
	gin.SetMode(gin.TestMode)
	for _, requestBody := range []string{
		"lines=0",
		"lines=1001",
		"lines=all",
		"scope=sshd",
	} {
		t.Run(requestBody, func(t *testing.T) {
			journalctlCalled := false
			server := &Server{
				systemCommand: func(name string, _ ...string) ([]byte, error) {
					if name == "journalctl" {
						journalctlCalled = true
					}
					return nil, nil
				},
				sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
			}
			templates, err := web.GetTemplates()
			if err != nil {
				t.Fatal(err)
			}
			router := gin.New()
			router.SetHTMLTemplate(templates)
			router.POST("/logs/show", server.handleLogsShow)
			request := httptest.NewRequest(http.MethodPost, "/logs/show", strings.NewReader(requestBody))
			request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
			response := httptest.NewRecorder()
			router.ServeHTTP(response, request)

			if response.Code != http.StatusBadRequest {
				t.Fatalf("status = %d, want %d", response.Code, http.StatusBadRequest)
			}
			if journalctlCalled {
				t.Error("journalctl was called for an invalid request")
			}
		})
	}
}

func TestHandleLogsShowPassesBoundedArgumentsToJournalctl(t *testing.T) {
	gin.SetMode(gin.TestMode)
	var gotName string
	var gotArgs []string
	server := &Server{
		systemCommand: func(name string, args ...string) ([]byte, error) {
			if name == "journalctl" {
				gotName = name
				gotArgs = append([]string(nil), args...)
			}
			return []byte("test log"), nil
		},
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
	}
	templates, err := web.GetTemplates()
	if err != nil {
		t.Fatal(err)
	}
	router := gin.New()
	router.SetHTMLTemplate(templates)
	router.POST("/logs/show", server.handleLogsShow)
	request := httptest.NewRequest(http.MethodPost, "/logs/show", strings.NewReader("lines=200&scope=piscsi-web"))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	response := httptest.NewRecorder()
	router.ServeHTTP(response, request)

	if response.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", response.Code, http.StatusOK)
	}
	if gotName != "journalctl" {
		t.Fatalf("command = %q, want journalctl", gotName)
	}
	if got, want := strings.Join(gotArgs, ","), "--no-pager,--lines=200,--unit=piscsi-web"; got != want {
		t.Errorf("arguments = %q, want %q", got, want)
	}
}
