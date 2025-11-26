// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"github.com/gin-gonic/gin"
	"github.com/gorilla/sessions"
)

const sessionName = "piscsi_session"

// getSession retrieves the session for a request
// This is a helper method for handlers that need to access the session. An
// invalid client cookie is treated as a new session so an expired or rotated
// session key does not prevent the browser from using the application.
func (s *Server) getSession(c *gin.Context) (*sessions.Session, error) {
	session, err := s.sessionStore.Get(c.Request, sessionName)
	if err != nil && session != nil {
		if s.logger != nil {
			s.logger.Warn("Discarding invalid session cookie", "error", err)
		}
		return session, nil
	}
	return session, err
}
