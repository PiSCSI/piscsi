// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package web

import (
	"embed"
	"fmt"
	"html/template"
	"io/fs"
	"net/http"
	"path"
	"strings"
)

// content contains the browser UI so release binaries do not depend on their
// working directory at runtime.
//
//go:embed templates/*.html static translations/*/LC_MESSAGES/messages.po
var content embed.FS

// GetTemplates parses all embedded HTML templates.
func GetTemplates() (*template.Template, error) {
	return template.ParseFS(content, "templates/*.html")
}

// GetStaticFS returns the embedded static asset tree.
func GetStaticFS() (http.FileSystem, error) {
	staticFS, err := fs.Sub(content, "static")
	if err != nil {
		return nil, fmt.Errorf("open embedded static assets: %w", err)
	}
	return http.FS(staticFS), nil
}

// GetPWAFile reads a single embedded PWA asset.
func GetPWAFile(name string) ([]byte, error) {
	cleanName, err := CleanPWAPath(name)
	if err != nil {
		return nil, err
	}

	data, err := content.ReadFile(path.Join("static/pwa", cleanName))
	if err != nil {
		return nil, fmt.Errorf("read embedded PWA asset %q: %w", cleanName, err)
	}
	return data, nil
}

// CleanPWAPath validates a PWA asset path and returns its relative, cleaned
// form for use with either embedded assets or the filesystem fallback.
func CleanPWAPath(name string) (string, error) {
	name = strings.TrimPrefix(name, "/")
	cleanName := path.Clean(name)
	if cleanName == "." || cleanName == ".." || path.IsAbs(cleanName) || strings.HasPrefix(cleanName, "../") ||
		strings.Contains(cleanName, `\`) {
		return "", fmt.Errorf("invalid PWA asset path %q", name)
	}

	return cleanName, nil
}
