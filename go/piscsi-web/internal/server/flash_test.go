package server

import (
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/gorilla/sessions"
)

func TestSetFlash(t *testing.T) {
	store := sessions.NewCookieStore([]byte("test-secret-key"))
	req := httptest.NewRequest("GET", "/", nil)
	session, _ := store.Get(req, "test_session")

	tests := []struct {
		name     string
		message  string
		category string
	}{
		{"success message", "Operation successful", FlashSuccess},
		{"error message", "Operation failed", FlashError},
		{"warning message", "Proceed with caution", FlashWarning},
		{"info message", "FYI", FlashInfo},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			SetFlash(session, tt.message, tt.category)

			flashes, ok := session.Values[flashSessionKey].([]FlashMessage)
			if !ok {
				t.Fatal("flash messages not stored correctly")
			}

			// Find the message we just added
			found := false
			for _, flash := range flashes {
				if flash.Message == tt.message && flash.Category == tt.category {
					found = true
					break
				}
			}

			if !found {
				t.Errorf("flash message %q with category %q not found in session", tt.message, tt.category)
			}
		})
	}
}

func TestSetFlash_Multiple(t *testing.T) {
	store := sessions.NewCookieStore([]byte("test-secret-key"))
	req := httptest.NewRequest("GET", "/", nil)
	session, _ := store.Get(req, "test_session")

	// Add multiple flash messages
	SetFlash(session, "First message", FlashSuccess)
	SetFlash(session, "Second message", FlashError)
	SetFlash(session, "Third message", FlashInfo)

	flashes, ok := session.Values[flashSessionKey].([]FlashMessage)
	if !ok {
		t.Fatal("flash messages not stored correctly")
	}

	if len(flashes) != 3 {
		t.Errorf("expected 3 flash messages, got %d", len(flashes))
	}

	// Verify order is maintained
	if flashes[0].Message != "First message" {
		t.Errorf("expected first message to be 'First message', got %q", flashes[0].Message)
	}
	if flashes[1].Message != "Second message" {
		t.Errorf("expected second message to be 'Second message', got %q", flashes[1].Message)
	}
	if flashes[2].Message != "Third message" {
		t.Errorf("expected third message to be 'Third message', got %q", flashes[2].Message)
	}
}

func TestGetFlashes(t *testing.T) {
	store := sessions.NewCookieStore([]byte("test-secret-key"))
	req := httptest.NewRequest("GET", "/", nil)
	session, _ := store.Get(req, "test_session")

	// Add some flash messages
	SetFlash(session, "Message 1", FlashSuccess)
	SetFlash(session, "Message 2", FlashError)

	// Get flashes
	flashes := GetFlashes(session)

	if len(flashes) != 2 {
		t.Errorf("expected 2 flash messages, got %d", len(flashes))
	}

	// Verify messages cleared from session (one-time read)
	_, exists := session.Values[flashSessionKey]
	if exists {
		t.Error("flash messages should be cleared from session after reading")
	}

	// Second call should return empty
	flashes2 := GetFlashes(session)
	if len(flashes2) != 0 {
		t.Errorf("expected 0 flash messages on second read, got %d", len(flashes2))
	}
}

func TestGetFlashes_Empty(t *testing.T) {
	store := sessions.NewCookieStore([]byte("test-secret-key"))
	req := httptest.NewRequest("GET", "/", nil)
	session, _ := store.Get(req, "test_session")

	// Get flashes from empty session
	flashes := GetFlashes(session)

	if flashes != nil {
		t.Errorf("expected nil for empty session, got %v", flashes)
	}
}

func TestGetFlashesForTemplate(t *testing.T) {
	store := sessions.NewCookieStore([]byte("test-secret-key"))

	tests := []struct {
		name              string
		messages          []FlashMessage
		expectedFlash     string
		expectedError     string
	}{
		{
			name: "success only",
			messages: []FlashMessage{
				{Message: "Success 1", Category: FlashSuccess},
			},
			expectedFlash: "Success 1",
			expectedError: "",
		},
		{
			name: "error only",
			messages: []FlashMessage{
				{Message: "Error 1", Category: FlashError},
			},
			expectedFlash: "",
			expectedError: "Error 1",
		},
		{
			name: "multiple success",
			messages: []FlashMessage{
				{Message: "Success 1", Category: FlashSuccess},
				{Message: "Success 2", Category: FlashSuccess},
			},
			expectedFlash: "Success 1\nSuccess 2",
			expectedError: "",
		},
		{
			name: "multiple errors",
			messages: []FlashMessage{
				{Message: "Error 1", Category: FlashError},
				{Message: "Error 2", Category: FlashError},
			},
			expectedFlash: "",
			expectedError: "Error 1\nError 2",
		},
		{
			name: "mixed messages",
			messages: []FlashMessage{
				{Message: "Success 1", Category: FlashSuccess},
				{Message: "Error 1", Category: FlashError},
				{Message: "Warning 1", Category: FlashWarning},
				{Message: "Info 1", Category: FlashInfo},
			},
			expectedFlash: "Success 1\nWarning 1\nInfo 1",
			expectedError: "Error 1",
		},
		{
			name:          "empty",
			messages:      []FlashMessage{},
			expectedFlash: "",
			expectedError: "",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create fresh session for each test
			req := httptest.NewRequest("GET", "/", nil)
			// Remove spaces from session name (spaces not allowed in cookie names)
			sessionName := "test_session_" + strings.ReplaceAll(tt.name, " ", "_")
			session, err := store.Get(req, sessionName)
			if err != nil {
				t.Fatalf("failed to get session: %v", err)
			}

			// Add messages
			for _, msg := range tt.messages {
				SetFlash(session, msg.Message, msg.Category)
			}

			// Get formatted messages
			flashMessage, errorMessage := GetFlashesForTemplate(session)

			if flashMessage != tt.expectedFlash {
				t.Errorf("flashMessage: expected %q, got %q", tt.expectedFlash, flashMessage)
			}

			if errorMessage != tt.expectedError {
				t.Errorf("errorMessage: expected %q, got %q", tt.expectedError, errorMessage)
			}

			// Verify messages are cleared
			_, exists := session.Values[flashSessionKey]
			if exists {
				t.Error("flash messages should be cleared from session after GetFlashesForTemplate")
			}
		})
	}
}

func TestFlashMessage_GobSerialization(t *testing.T) {
	// This test ensures that FlashMessage can be serialized by gob
	// which is required for session storage
	store := sessions.NewCookieStore([]byte("test-secret-key"))
	req := httptest.NewRequest("GET", "/", nil)
	w := httptest.NewRecorder()

	session, _ := store.Get(req, "test_session")

	// Add a flash message
	SetFlash(session, "Test message", FlashSuccess)

	// Save the session (this will serialize with gob)
	err := session.Save(req, w)
	if err != nil {
		t.Fatalf("failed to save session with flash messages: %v", err)
	}

	// Verify the Set-Cookie header is present
	cookies := w.Result().Cookies()
	if len(cookies) == 0 {
		t.Fatal("expected session cookie to be set")
	}
}
