// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"bytes"
	"compress/gzip"
	"context"
	"errors"
	"fmt"
	"html/template"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
)

var systemManpageDirs = map[int][]string{
	1: {
		"/usr/local/man/man1",
		"/usr/local/share/man/man1",
		"/usr/man/man1",
		"/usr/share/man/man1",
	},
	8: {
		"/usr/local/man/man8",
		"/usr/local/share/man/man8",
		"/usr/man/man8",
		"/usr/share/man/man8",
	},
}

var manpageLinkPattern = regexp.MustCompile(`(?i)href="/\?1\+([a-z0-9_-]+)"`)

// manpage describes a PiSCSI manual page that can be displayed in the web UI.
type manpage struct {
	App         string
	Section     int
	Description string
}

var piscsiManpages = []manpage{
	{App: "piscsi", Section: 1, Description: "Emulates SCSI devices using the Raspberry Pi GPIO pins."},
	{App: "piscsi-web", Section: 1, Description: "Web control interface for PiSCSI."},
	{App: "piscsi-oled", Section: 1, Description: "Displays PiSCSI status on an SSD1306 OLED panel."},
	{App: "piscsi-ctrlboard", Section: 1, Description: "Operates PiSCSI with the PiSCSI Control Board."},
	{App: "sasidump", Section: 1, Description: "SASI disk dumping tool for PiSCSI."},
	{App: "scsictl", Section: 1, Description: "Sends management commands to the piscsi process."},
	{App: "scsidump", Section: 1, Description: "SCSI disk dumping tool for PiSCSI."},
	{App: "scsiloop", Section: 1, Description: "Tests a PiSCSI board with a loopback adapter."},
	{App: "scsimon", Section: 1, Description: "Captures traffic on the SCSI bus."},
	{App: "piscsi-network-profile", Section: 8, Description: "Manages PiSCSI DaynaPort network profiles."},
}

// findSystemManpage finds a source manpage in the system locations used by
// PiSCSI and the common Linux man implementations.
func findSystemManpage(app string, section int, dirs []string) (string, error) {
	suffix := fmt.Sprintf(".%d", section)
	for _, dir := range dirs {
		for _, extension := range []string{suffix, suffix + ".gz"} {
			path := filepath.Join(dir, app+extension)
			info, err := os.Stat(path)
			if err == nil && info.Mode().IsRegular() {
				return path, nil
			}
		}
	}

	return "", fmt.Errorf("manual page for %s(%d) is not installed", app, section)
}

func findPiSCSIManpage(app string) (manpage, bool) {
	for _, page := range piscsiManpages {
		if page.App == app {
			return page, true
		}
	}

	return manpage{}, false
}

func readRoffManpage(path string) ([]byte, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	var reader io.Reader = file
	if strings.HasSuffix(path, ".gz") {
		gzipReader, err := gzip.NewReader(file)
		if err != nil {
			return nil, fmt.Errorf("open compressed manual page: %w", err)
		}
		defer gzipReader.Close()
		reader = gzipReader
	}

	roff, err := io.ReadAll(reader)
	if err != nil {
		return nil, fmt.Errorf("read manual page: %w", err)
	}

	return roff, nil
}

func renderRoffManpage(ctx context.Context, roff []byte) (template.HTML, error) {
	renderers := []struct {
		name string
		args []string
	}{
		{name: "mandoc", args: []string{"-Thtml"}},
		{name: "man2html", args: []string{"-M", "/"}},
		{name: "groff", args: []string{"-mandoc", "-Thtml"}},
	}

	var rendererErrors []error
	for _, renderer := range renderers {
		if _, err := exec.LookPath(renderer.name); err != nil {
			continue
		}

		cmd := exec.CommandContext(ctx, renderer.name, renderer.args...)
		cmd.Stdin = bytes.NewReader(roff)
		output, err := cmd.CombinedOutput()
		if err != nil {
			rendererErrors = append(rendererErrors,
				fmt.Errorf("%s: %w: %s", renderer.name, err, strings.TrimSpace(string(output))))
			continue
		}

		body, err := manpageHTMLBody(string(output))
		if err != nil {
			rendererErrors = append(rendererErrors, fmt.Errorf("%s: %w", renderer.name, err))
			continue
		}

		// man2html uses CGI-style links for references to other section 1
		// pages. Route those references back through this endpoint.
		body = manpageLinkPattern.ReplaceAllString(body, `href="/sys/manpage?app=$1"`)
		return template.HTML(body), nil
	}

	if len(rendererErrors) > 0 {
		return "", errors.Join(rendererErrors...)
	}

	return "", errors.New("no roff-to-HTML renderer is installed (install mandoc, man2html, or groff)")
}

func manpageHTMLBody(document string) (string, error) {
	lowerDocument := strings.ToLower(document)
	bodyStart := strings.Index(lowerDocument, "<body")
	if bodyStart == -1 {
		return "", errors.New("renderer output does not contain a body element")
	}

	contentStartOffset := strings.Index(lowerDocument[bodyStart:], ">")
	if contentStartOffset == -1 {
		return "", errors.New("renderer output contains an invalid body element")
	}
	contentStart := bodyStart + contentStartOffset + 1

	bodyEndOffset := strings.Index(lowerDocument[contentStart:], "</body>")
	if bodyEndOffset == -1 {
		return "", errors.New("renderer output does not close the body element")
	}

	return strings.TrimSpace(document[contentStart : contentStart+bodyEndOffset]), nil
}
