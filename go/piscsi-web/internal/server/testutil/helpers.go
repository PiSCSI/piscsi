package testutil

import (
	"net/http"
	"net/http/cookiejar"
	"net/http/httptest"
	"net/url"
	"strings"
	"testing"

	"github.com/gorilla/sessions"
)

// TestClient wraps an HTTP client with helper methods for testing
type TestClient struct {
	*http.Client
	BaseURL string
	t       *testing.T
}

// NewTestClient creates a new test client with a cookie jar for session management
func NewTestClient(t *testing.T, baseURL string) *TestClient {
	jar, err := cookiejar.New(nil)
	if err != nil {
		t.Fatalf("failed to create cookie jar: %v", err)
	}

	return &TestClient{
		Client: &http.Client{
			Jar: jar,
			CheckRedirect: func(req *http.Request, via []*http.Request) error {
				// Don't follow redirects automatically - we want to inspect them
				return http.ErrUseLastResponse
			},
		},
		BaseURL: baseURL,
		t:       t,
	}
}

// Get performs a GET request
func (c *TestClient) Get(path string) *http.Response {
	req, err := http.NewRequest("GET", c.BaseURL+path, nil)
	if err != nil {
		c.t.Fatalf("failed to create GET request: %v", err)
	}

	resp, err := c.Do(req)
	if err != nil {
		c.t.Fatalf("GET request failed: %v", err)
	}

	return resp
}

// PostForm performs a POST request with form data
func (c *TestClient) PostForm(path string, data url.Values) *http.Response {
	req, err := http.NewRequest("POST", c.BaseURL+path, strings.NewReader(data.Encode()))
	if err != nil {
		c.t.Fatalf("failed to create POST request: %v", err)
	}
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")

	resp, err := c.Do(req)
	if err != nil {
		c.t.Fatalf("POST request failed: %v", err)
	}

	return resp
}

// Assertion helpers

// AssertStatusCode checks if the response has the expected status code
func AssertStatusCode(t *testing.T, resp *http.Response, expected int) {
	t.Helper()
	if resp.StatusCode != expected {
		t.Errorf("expected status code %d, got %d", expected, resp.StatusCode)
	}
}

// AssertRedirect checks if the response is a redirect to the expected location
func AssertRedirect(t *testing.T, resp *http.Response, expectedLocation string) {
	t.Helper()
	if resp.StatusCode != http.StatusSeeOther && resp.StatusCode != http.StatusFound {
		t.Errorf("expected redirect status (303 or 302), got %d", resp.StatusCode)
	}
	location := resp.Header.Get("Location")
	if location != expectedLocation {
		t.Errorf("expected redirect to %q, got %q", expectedLocation, location)
	}
}

// AssertCookieExists checks if a cookie with the given name exists in the response
func AssertCookieExists(t *testing.T, resp *http.Response, cookieName string) {
	t.Helper()
	found := false
	for _, cookie := range resp.Cookies() {
		if cookie.Name == cookieName {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected cookie %q to exist in response", cookieName)
	}
}

// AssertNoCookie checks that a cookie with the given name does not exist
func AssertNoCookie(t *testing.T, resp *http.Response, cookieName string) {
	t.Helper()
	for _, cookie := range resp.Cookies() {
		if cookie.Name == cookieName {
			t.Errorf("expected cookie %q to not exist in response", cookieName)
		}
	}
}

// GetSessionFromResponse extracts the session from response cookies
// This is a helper for testing flash messages
func GetSessionFromResponse(t *testing.T, resp *http.Response, store *sessions.CookieStore, req *http.Request) *sessions.Session {
	t.Helper()

	// Create a test request with the response cookies
	testReq := httptest.NewRequest("GET", "/", nil)
	for _, cookie := range resp.Cookies() {
		testReq.AddCookie(cookie)
	}

	session, err := store.Get(testReq, "piscsi_session")
	if err != nil {
		t.Fatalf("failed to get session from response: %v", err)
	}

	return session
}

// AssertContains checks if a string contains a substring
func AssertContains(t *testing.T, haystack, needle string) {
	t.Helper()
	if !strings.Contains(haystack, needle) {
		t.Errorf("expected string to contain %q, but it doesn't.\nGot: %s", needle, haystack)
	}
}

// AssertNotContains checks if a string does not contain a substring
func AssertNotContains(t *testing.T, haystack, needle string) {
	t.Helper()
	if strings.Contains(haystack, needle) {
		t.Errorf("expected string to not contain %q, but it does.\nGot: %s", needle, haystack)
	}
}

// AssertEqual checks if two values are equal
func AssertEqual(t *testing.T, expected, actual interface{}) {
	t.Helper()
	if expected != actual {
		t.Errorf("expected %v, got %v", expected, actual)
	}
}

// AssertNotEqual checks if two values are not equal
func AssertNotEqual(t *testing.T, expected, actual interface{}) {
	t.Helper()
	if expected == actual {
		t.Errorf("expected values to not be equal, but both are %v", expected)
	}
}
