package server

import (
	"net/http"
	"net/http/httptest"
	"net/url"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
	"github.com/piscsi/piscsi-web/web"
	xhtml "golang.org/x/net/html"
)

func TestBrowserSupportsModernTheme(t *testing.T) {
	tests := []struct {
		name      string
		userAgent string
		want      bool
	}{
		{"current Chrome", "Mozilla/5.0 Chrome/126.0.0.0 Safari/537.36", true},
		{"old Chrome", "Mozilla/5.0 Chrome/99.0.0.0 Safari/537.36", false},
		{"current Safari", "Mozilla/5.0 Version/17.5 Safari/605.1.15", true},
		{"old Safari", "Mozilla/5.0 Version/13.1 Safari/605.1.15", false},
		{"current Firefox", "Mozilla/5.0 Firefox/128.0", true},
		{"unknown", "MacWeb/2.0", false},
		{"missing", "", false},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			if got := browserSupportsModernTheme(test.userAgent); got != test.want {
				t.Errorf("browserSupportsModernTheme() = %t, want %t", got, test.want)
			}
		})
	}
}

func TestSelectedLocaleDetectionAndSessionOverride(t *testing.T) {
	gin.SetMode(gin.TestMode)
	store := sessions.NewCookieStore([]byte("test-secret-key"))
	server := &Server{sessionStore: store}

	request := httptest.NewRequest("GET", "/", nil)
	request.Header.Set("Accept-Language", "sv-SE,sv;q=0.9,en;q=0.8")
	context, _ := gin.CreateTestContext(httptest.NewRecorder())
	context.Request = request
	if got := server.selectedLocale(context); got != "sv" {
		t.Fatalf("detected locale = %q, want sv", got)
	}

	sessionResponse := httptest.NewRecorder()
	session, err := store.Get(request, sessionName)
	if err != nil {
		t.Fatal(err)
	}
	session.Values["language"] = "de"
	if err := session.Save(request, sessionResponse); err != nil {
		t.Fatal(err)
	}
	for _, cookie := range sessionResponse.Result().Cookies() {
		request.AddCookie(cookie)
	}
	if got := server.selectedLocale(context); got != "de" {
		t.Fatalf("session locale = %q, want de", got)
	}
}

func TestSelectedThemePreservesSessionOverride(t *testing.T) {
	gin.SetMode(gin.TestMode)
	store := sessions.NewCookieStore([]byte("test-secret-key"))
	server := &Server{sessionStore: store}

	request := httptest.NewRequest("GET", "/", nil)
	request.Header.Set("User-Agent", "MacWeb/2.0")
	sessionResponse := httptest.NewRecorder()
	session, err := store.Get(request, sessionName)
	if err != nil {
		t.Fatal(err)
	}
	session.Values["theme"] = "modern"
	if err := session.Save(request, sessionResponse); err != nil {
		t.Fatal(err)
	}
	for _, cookie := range sessionResponse.Result().Cookies() {
		request.AddCookie(cookie)
	}
	context, _ := gin.CreateTestContext(httptest.NewRecorder())
	context.Request = request
	if got := server.selectedTheme(context); got != "modern" {
		t.Fatalf("session theme = %q, want modern", got)
	}
}

func TestHandleLanguageRejectsUnsupportedLocale(t *testing.T) {
	gin.SetMode(gin.TestMode)
	server := &Server{
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
	}
	form := url.Values{"locale": {"it"}}
	request := httptest.NewRequest(http.MethodPost, "/language", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	recorder := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(recorder)
	context.Request = request

	server.handleLanguage(context)

	if recorder.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, want %d", recorder.Code, http.StatusSeeOther)
	}
	cookies := recorder.Result().Cookies()
	if len(cookies) == 0 {
		t.Fatal("theme response did not set a session cookie")
	}
	redirectRequest := httptest.NewRequest(http.MethodGet, "/", nil)
	redirectRequest.AddCookie(cookies[len(cookies)-1])
	session, err := server.sessionStore.Get(redirectRequest, sessionName)
	if err != nil {
		t.Fatal(err)
	}
	_, errorMessage := GetFlashesForTemplate(session)
	if !strings.Contains(errorMessage, "not supported") {
		t.Fatalf("error flash = %q", errorMessage)
	}
}

func TestHandleThemeReturnsExactSuccessMessage(t *testing.T) {
	gin.SetMode(gin.TestMode)
	server := &Server{
		sessionStore: sessions.NewCookieStore([]byte("test-secret-key")),
	}
	form := url.Values{"theme": {"modern"}}
	request := httptest.NewRequest(http.MethodPost, "/theme", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	recorder := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(recorder)
	context.Request = request

	server.handleTheme(context)

	if recorder.Code != http.StatusSeeOther {
		t.Fatalf("status = %d, want %d", recorder.Code, http.StatusSeeOther)
	}
	cookies := recorder.Result().Cookies()
	if len(cookies) == 0 {
		t.Fatal("theme response did not set a session cookie")
	}
	redirectRequest := httptest.NewRequest(http.MethodGet, "/", nil)
	redirectRequest.AddCookie(cookies[len(cookies)-1])
	session, err := server.sessionStore.Get(redirectRequest, sessionName)
	if err != nil {
		t.Fatal(err)
	}
	message, _ := GetFlashesForTemplate(session)
	if message != "Theme changed to 'modern'." {
		t.Fatalf("message = %q, want %q", message, "Theme changed to 'modern'.")
	}
}

func TestThemeFlashRendersWithoutSuffix(t *testing.T) {
	gin.SetMode(gin.TestMode)
	store := sessions.NewCookieStore([]byte("test-secret-key"))
	server := &Server{sessionStore: store}
	form := url.Values{"theme": {"modern"}}
	request := httptest.NewRequest(http.MethodPost, "/theme", strings.NewReader(form.Encode()))
	request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	recorder := httptest.NewRecorder()
	context, _ := gin.CreateTestContext(recorder)
	context.Request = request

	server.handleTheme(context)

	cookies := recorder.Result().Cookies()
	if len(cookies) == 0 {
		t.Fatal("theme response did not set a session cookie")
	}
	redirectRequest := httptest.NewRequest(http.MethodGet, "/sys/admin", nil)
	redirectRequest.AddCookie(cookies[len(cookies)-1])
	session, err := store.Get(redirectRequest, sessionName)
	if err != nil {
		t.Fatalf("read redirected session: %v", err)
	}
	flashMessage, _ := GetFlashesForTemplate(session)

	templates, err := web.GetTemplates()
	if err != nil {
		t.Fatalf("load templates: %v", err)
	}
	catalog, err := web.LoadCatalog()
	if err != nil {
		t.Fatalf("load translations: %v", err)
	}
	renderer := web.LocalizedRenderer{Template: templates, Catalog: catalog}
	rendered := httptest.NewRecorder()
	instance := renderer.Instance("admin.html", gin.H{
		"Title":        "PiSCSI Control",
		"Locale":       "en",
		"Theme":        "modern",
		"FlashMessage": flashMessage,
	})
	instance.WriteContentType(rendered)
	if err := instance.Render(rendered); err != nil {
		t.Fatalf("render theme response: %v", err)
	}

	document, err := xhtml.Parse(strings.NewReader(rendered.Body.String()))
	if err != nil {
		t.Fatalf("parse rendered response: %v", err)
	}
	flash := findElementByID(document, "flash")
	if flash == nil {
		t.Fatal("rendered response does not contain the flash element")
	}
	if got := strings.TrimSpace(elementText(flash)); got != "Theme changed to 'modern'." {
		t.Fatalf("rendered flash = %q, want %q", got, "Theme changed to 'modern'.")
	}
}

func findElementByID(node *xhtml.Node, id string) *xhtml.Node {
	if node.Type == xhtml.ElementNode {
		for _, attribute := range node.Attr {
			if attribute.Key == "id" && attribute.Val == id {
				return node
			}
		}
	}
	for child := node.FirstChild; child != nil; child = child.NextSibling {
		if match := findElementByID(child, id); match != nil {
			return match
		}
	}
	return nil
}

func elementText(node *xhtml.Node) string {
	if node.Type == xhtml.TextNode {
		return node.Data
	}
	var text strings.Builder
	for child := node.FirstChild; child != nil; child = child.NextSibling {
		text.WriteString(elementText(child))
	}
	return text.String()
}
