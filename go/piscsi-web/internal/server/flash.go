// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"encoding/gob"

	"github.com/gorilla/sessions"
)

func init() {
	// Register FlashMessage type with gob for session serialization
	gob.Register([]FlashMessage{})
}

// FlashMessage represents a flash message to be displayed to the user
type FlashMessage struct {
	Message  string
	Category string
}

const (
	// Flash message categories
	FlashSuccess = "success"
	FlashError   = "error"
	FlashWarning = "warning"
	FlashInfo    = "info"
)

const flashSessionKey = "flash_messages"

// SetFlash adds a flash message to the session
func SetFlash(session *sessions.Session, message, category string) {
	// Get existing flashes or create new slice
	var flashes []FlashMessage
	if flashesRaw, ok := session.Values[flashSessionKey]; ok {
		if flashesList, ok := flashesRaw.([]FlashMessage); ok {
			flashes = flashesList
		}
	}

	// Add new flash message
	flashes = append(flashes, FlashMessage{
		Message:  message,
		Category: category,
	})

	// Save to session
	session.Values[flashSessionKey] = flashes
}

// GetFlashes retrieves and clears flash messages from the session
func GetFlashes(session *sessions.Session) []FlashMessage {
	// Get flashes from session
	flashesRaw, ok := session.Values[flashSessionKey]
	if !ok {
		return nil
	}

	flashes, ok := flashesRaw.([]FlashMessage)
	if !ok {
		return nil
	}

	// Clear flashes from session (one-time read)
	delete(session.Values, flashSessionKey)

	return flashes
}

// GetFlashesForTemplate converts flash messages to template-friendly format
// Returns separate FlashMessage and ErrorMessage strings for compatibility
func GetFlashesForTemplate(session *sessions.Session) (flashMessage, errorMessage string) {
	flashes := GetFlashes(session)

	for _, flash := range flashes {
		switch flash.Category {
		case FlashError:
			if errorMessage != "" {
				errorMessage += "\n"
			}
			errorMessage += flash.Message
		default:
			// Success, info, warning all go to flash message
			if flashMessage != "" {
				flashMessage += "\n"
			}
			flashMessage += flash.Message
		}
	}

	return flashMessage, errorMessage
}
