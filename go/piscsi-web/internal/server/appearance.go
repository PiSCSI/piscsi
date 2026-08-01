// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"regexp"
	"strconv"

	"github.com/gin-gonic/gin"
	"github.com/piscsi/piscsi/go/piscsi-web/web"
)

var modernBrowserPatterns = []struct {
	expression *regexp.Regexp
	minimum    int
}{
	{regexp.MustCompile(`(?:Edg|Edge)/(\d+)`), 100},
	{regexp.MustCompile(`(?:Chrome|CriOS)/(\d+)`), 100},
	{regexp.MustCompile(`(?:Firefox|FxiOS)/(\d+)`), 100},
	{regexp.MustCompile(`Version/(\d+).*(?:Mobile/.+ )?Safari/`), 14},
}

func (s *Server) selectedLocale(c *gin.Context) string {
	if c == nil {
		return "en"
	}
	if s != nil && s.sessionStore != nil {
		session, err := s.getSession(c)
		if err == nil {
			if selected, ok := session.Values["language"].(string); ok && web.IsSupportedLocale(selected) {
				return selected
			}
		}
	}
	return web.MatchLocale(c.GetHeader("Accept-Language"))
}

func (s *Server) selectedTheme(c *gin.Context) string {
	if c != nil {
		if s != nil && s.sessionStore != nil {
			session, err := s.getSession(c)
			if err == nil {
				if selected, ok := session.Values["theme"].(string); ok &&
					(selected == "modern" || selected == "classic") {
					return selected
				}
			}
		}
		if browserSupportsModernTheme(c.GetHeader("User-Agent")) {
			return "modern"
		}
	}
	return "classic"
}

func browserSupportsModernTheme(userAgent string) bool {
	if userAgent == "" {
		return false
	}
	for _, browser := range modernBrowserPatterns {
		match := browser.expression.FindStringSubmatch(userAgent)
		if len(match) != 2 {
			continue
		}
		version, err := strconv.Atoi(match[1])
		return err == nil && version >= browser.minimum
	}
	return false
}
