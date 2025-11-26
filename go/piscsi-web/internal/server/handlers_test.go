package server

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi-web/internal/config"
)

func TestRespond_HTMLMode_RedirectWithFlash(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	server := &Server{
		sessionStore: store,
	}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("POST", "/test", nil)

	server.respond(c, ResponseOptions{
		Message: "Device attached successfully",
	})

	resp := w.Result()

	// Verify Location header is set (status code handling requires full Gin router in integration tests)
	location := resp.Header.Get("Location")
	if location != "/" {
		t.Errorf("expected redirect to '/', got '%s'", location)
	}

	// Verify session cookie was set
	cookies := resp.Cookies()
	if len(cookies) == 0 {
		t.Fatal("expected session cookie to be set")
	}

	// Extract and verify flash message from session
	sessionCookie := cookies[0]
	req := httptest.NewRequest("GET", "/", nil)
	req.AddCookie(sessionCookie)

	session, err := store.Get(req, "piscsi_session")
	if err != nil {
		t.Fatalf("failed to get session: %v", err)
	}

	flashMessage, _ := GetFlashesForTemplate(session)
	if !strings.Contains(flashMessage, "Device attached successfully") {
		t.Errorf("expected flash message to contain 'Device attached successfully', got '%s'", flashMessage)
	}
}

func TestRespond_HTMLMode_RedirectWithError(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	server := &Server{
		sessionStore: store,
	}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("POST", "/test", nil)

	server.respond(c, ResponseOptions{
		Error:   true,
		Message: "Failed to attach device",
	})

	resp := w.Result()

	// Verify Location header is set (redirects even for errors)
	location := resp.Header.Get("Location")
	if location != "/" {
		t.Errorf("expected redirect to '/', got '%s'", location)
	}

	// Verify session cookie
	cookies := resp.Cookies()
	if len(cookies) == 0 {
		t.Fatal("expected session cookie to be set")
	}

	// Extract and verify error message from session
	sessionCookie := cookies[0]
	req := httptest.NewRequest("GET", "/", nil)
	req.AddCookie(sessionCookie)

	session, err := store.Get(req, "piscsi_session")
	if err != nil {
		t.Fatalf("failed to get session: %v", err)
	}

	_, errorMessage := GetFlashesForTemplate(session)
	if !strings.Contains(errorMessage, "Failed to attach device") {
		t.Errorf("expected error message to contain 'Failed to attach device', got '%s'", errorMessage)
	}
}

func TestRespond_HTMLMode_CustomRedirect(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	server := &Server{
		sessionStore: store,
	}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("POST", "/test", nil)

	server.respond(c, ResponseOptions{
		Message:     "Configuration saved",
		RedirectURL: "/config",
	})

	resp := w.Result()

	// Verify custom redirect URL
	location := resp.Header.Get("Location")
	if location != "/config" {
		t.Errorf("expected redirect to '/config', got '%s'", location)
	}
}

func TestRespond_HTMLMode_TemplateRender(t *testing.T) {
	t.Skip("Template rendering requires full server setup with template engine")
	// TODO: Implement integration test for template rendering mode
}

func TestRespondIgnoresJSONAcceptHeader(t *testing.T) {
	gin.SetMode(gin.TestMode)

	store := sessions.NewCookieStore([]byte("test-secret-key"))
	server := &Server{
		sessionStore: store,
	}

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Request = httptest.NewRequest("POST", "/test", nil)
	c.Request.Header.Set("Accept", "application/json")

	server.respond(c, ResponseOptions{Message: "Operation successful"})

	if w.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, want %d", w.Code, http.StatusSeeOther)
	}
	if got := w.Header().Get("Location"); got != "/" {
		t.Errorf("Location = %q, want /", got)
	}
	if got := w.Header().Get("Content-Type"); strings.Contains(got, "application/json") {
		t.Errorf("Content-Type = %q, JSON negotiation must not be supported", got)
	}
}

func TestHealthcheckIsTheIntentionalJSONEndpoint(t *testing.T) {
	gin.SetMode(gin.TestMode)

	recorder := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(recorder)
	context.Request = httptest.NewRequest(http.MethodGet, "/healthcheck", nil)

	(&Server{}).handleHealthcheck(context)

	if recorder.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", recorder.Code, http.StatusOK)
	}
	if got := recorder.Header().Get("Content-Type"); !strings.Contains(got, "application/json") {
		t.Errorf("Content-Type = %q, want application/json", got)
	}
	if body := strings.TrimSpace(recorder.Body.String()); body != `{"status":"healthy"}` {
		t.Errorf("body = %q, want health status", body)
	}
}

func TestLegacyJSONOnlyRoutesAreNotRegistered(t *testing.T) {
	gin.SetMode(gin.TestMode)

	server := &Server{
		config: &config.Config{},
		router: gin.New(),
	}
	server.setupRoutes()

	routes := make(map[string]struct{})
	for _, route := range server.router.Routes() {
		routes[route.Method+" "+route.Path] = struct{}{}
	}
	if _, ok := routes["GET /healthcheck"]; !ok {
		t.Error("intentional healthcheck route is not registered")
	}
	for _, route := range []string{"GET /env", "GET /files/list", "GET /config/list"} {
		if _, ok := routes[route]; ok {
			t.Errorf("legacy JSON route %q is still registered", route)
		}
	}
}
