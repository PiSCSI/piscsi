package web

import (
	"bytes"
	"io"
	"strings"
	"testing"

	xhtml "golang.org/x/net/html"
)

func TestEmbeddedWebAssets(t *testing.T) {
	templates, err := GetTemplates()
	if err != nil {
		t.Fatalf("GetTemplates() error = %v", err)
	}
	if templates.Lookup("index.html") == nil {
		t.Fatal("index.html was not embedded")
	}
	var rendered bytes.Buffer
	if err := templates.ExecuteTemplate(&rendered, "index.html", map[string]interface{}{}); err != nil {
		t.Fatalf("render index.html: %v", err)
	}
	if strings.Contains(rendered.String(), "Go to Home") {
		t.Fatal("index.html contains a link to the page itself")
	}
	if !strings.Contains(rendered.String(), `/pwa/android-icon-192x192.png`) {
		t.Fatal("index.html does not reference the 192x192 PWA icon")
	}

	staticFS, err := GetStaticFS()
	if err != nil {
		t.Fatalf("GetStaticFS() error = %v", err)
	}
	logo, err := staticFS.Open("/logo.svg")
	if err != nil {
		t.Fatalf("open embedded logo: %v", err)
	}
	defer logo.Close()
	if _, err := io.ReadAll(logo); err != nil {
		t.Fatalf("read embedded logo: %v", err)
	}

	if _, err := GetPWAFile("/manifest.json"); err != nil {
		t.Fatalf("GetPWAFile() error = %v", err)
	}
}

func TestDiskInfoUsesWrappingTextContainer(t *testing.T) {
	templates, err := GetTemplates()
	if err != nil {
		t.Fatalf("GetTemplates() error = %v", err)
	}

	var rendered bytes.Buffer
	if err := templates.ExecuteTemplate(&rendered, "diskinfo.html", map[string]interface{}{
		"DiskInfo": "disk image details",
	}); err != nil {
		t.Fatalf("render diskinfo.html: %v", err)
	}

	html := rendered.String()
	if !strings.Contains(html, `<body class="page-diskinfo">`) {
		t.Fatal("diskinfo.html does not identify the disk info page")
	}
	if !strings.Contains(html, `<div class="text-container"><pre><samp>disk image details</samp></pre></div>`) {
		t.Fatal("diskinfo.html does not wrap the disk info text")
	}
}

func TestDiskInfoRetainsOutputWhitespace(t *testing.T) {
	templates, err := GetTemplates()
	if err != nil {
		t.Fatalf("GetTemplates() error = %v", err)
	}

	const diskInfo = "\n  first\tcolumn\nsecond  column\n"
	var rendered bytes.Buffer
	if err := templates.ExecuteTemplate(&rendered, "diskinfo.html", map[string]interface{}{
		"DiskInfo": diskInfo,
	}); err != nil {
		t.Fatalf("render diskinfo.html: %v", err)
	}

	document, err := xhtml.Parse(&rendered)
	if err != nil {
		t.Fatalf("parse diskinfo.html: %v", err)
	}

	var sample *xhtml.Node
	var findSample func(*xhtml.Node)
	findSample = func(node *xhtml.Node) {
		if node.Type == xhtml.ElementNode && node.Data == "samp" {
			sample = node
			return
		}
		for child := node.FirstChild; child != nil && sample == nil; child = child.NextSibling {
			findSample(child)
		}
	}
	findSample(document)
	if sample == nil || sample.FirstChild == nil {
		t.Fatal("diskinfo.html does not contain the disk info output")
	}
	if got := sample.FirstChild.Data; got != diskInfo {
		t.Errorf("disk info output = %q, want %q", got, diskInfo)
	}
}

func TestGetPWAFileRejectsTraversal(t *testing.T) {
	if _, err := GetPWAFile("../../templates/index.html"); err == nil {
		t.Fatal("GetPWAFile() accepted a traversal path")
	}
}

func TestCleanPWAPath(t *testing.T) {
	for _, name := range []string{
		"../favicon.ico",
		"../../templates/index.html",
		"//favicon.ico",
		`..\favicon.ico`,
	} {
		if _, err := CleanPWAPath(name); err == nil {
			t.Errorf("CleanPWAPath(%q) accepted an invalid path", name)
		}
	}

	if got, err := CleanPWAPath("/icons/../favicon.ico"); err != nil || got != "favicon.ico" {
		t.Errorf("CleanPWAPath() = %q, %v; want %q, nil", got, err, "favicon.ico")
	}
}
