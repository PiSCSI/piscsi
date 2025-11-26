package server

import (
	"errors"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi-web/web"
)

func TestParseServiceStatus(t *testing.T) {
	processes := `
afpd             /usr/sbin/afpd -d
smbd             /usr/sbin/smbd --foreground
python3          python3 /opt/macproxy/macproxy.py
perl             /usr/share/webmin/miniserv.pl /etc/webmin/miniserv.conf
`
	got := parseServiceStatus(processes)
	if !got.Netatalk || !got.Samba || !got.Macproxy || !got.Webmin {
		t.Fatalf("parseServiceStatus() = %+v, expected detected services", got)
	}
	if got.FTP {
		t.Fatalf("parseServiceStatus() = %+v, expected FTP to be disabled", got)
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
