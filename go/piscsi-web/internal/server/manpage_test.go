package server

import (
	"bytes"
	"compress/gzip"
	"context"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

func TestFindSystemManpage(t *testing.T) {
	manDir := t.TempDir()
	compressedPath := filepath.Join(manDir, "piscsi.1.gz")
	if err := os.WriteFile(compressedPath, []byte("compressed"), 0o644); err != nil {
		t.Fatal(err)
	}

	path, err := findSystemManpage("piscsi", []string{manDir})
	if err != nil {
		t.Fatalf("findSystemManpage() error = %v", err)
	}
	if path != compressedPath {
		t.Errorf("findSystemManpage() = %q, want %q", path, compressedPath)
	}

	uncompressedPath := filepath.Join(manDir, "piscsi.1")
	if err := os.WriteFile(uncompressedPath, []byte("uncompressed"), 0o644); err != nil {
		t.Fatal(err)
	}

	path, err = findSystemManpage("piscsi", []string{manDir})
	if err != nil {
		t.Fatalf("findSystemManpage() error = %v", err)
	}
	if path != uncompressedPath {
		t.Errorf("findSystemManpage() = %q, want %q", path, uncompressedPath)
	}
}

func TestFindSystemManpageNotInstalled(t *testing.T) {
	_, err := findSystemManpage("piscsi", []string{t.TempDir()})
	if err == nil || !strings.Contains(err.Error(), "not installed") {
		t.Fatalf("findSystemManpage() error = %v, want a not-installed error", err)
	}
}

func TestFindSystemManpageSearchesLaterDirectories(t *testing.T) {
	firstDir := t.TempDir()
	usrManDir := t.TempDir()
	want := filepath.Join(usrManDir, "piscsi.1")
	if err := os.WriteFile(want, []byte(".Dt PISCSI 1\n"), 0o644); err != nil {
		t.Fatal(err)
	}

	got, err := findSystemManpage("piscsi", []string{firstDir, usrManDir})
	if err != nil {
		t.Fatalf("findSystemManpage() error = %v", err)
	}
	if got != want {
		t.Errorf("findSystemManpage() = %q, want %q", got, want)
	}
}

func TestReadRoffManpageCompressed(t *testing.T) {
	var compressed bytes.Buffer
	writer := gzip.NewWriter(&compressed)
	if _, err := writer.Write([]byte(".Dt PISCSI 1\n")); err != nil {
		t.Fatal(err)
	}
	if err := writer.Close(); err != nil {
		t.Fatal(err)
	}

	path := filepath.Join(t.TempDir(), "piscsi.1.gz")
	if err := os.WriteFile(path, compressed.Bytes(), 0o644); err != nil {
		t.Fatal(err)
	}

	roff, err := readRoffManpage(path)
	if err != nil {
		t.Fatalf("readRoffManpage() error = %v", err)
	}
	if got, want := string(roff), ".Dt PISCSI 1\n"; got != want {
		t.Errorf("readRoffManpage() = %q, want %q", got, want)
	}
}

func TestManpageHTMLBody(t *testing.T) {
	document := "Content-type: text/html\n<HTML><HEAD><TITLE>Manual</TITLE></HEAD>" +
		`<BODY class="manual"><h1>NAME</h1><p>piscsi</p></BODY></HTML>`

	body, err := manpageHTMLBody(document)
	if err != nil {
		t.Fatalf("manpageHTMLBody() error = %v", err)
	}
	if got, want := body, "<h1>NAME</h1><p>piscsi</p>"; got != want {
		t.Errorf("manpageHTMLBody() = %q, want %q", got, want)
	}
}

func TestManpageHTMLBodyRejectsIncompleteDocument(t *testing.T) {
	if _, err := manpageHTMLBody("<html><p>piscsi</p></html>"); err == nil {
		t.Fatal("manpageHTMLBody() error = nil, want an invalid-document error")
	}
}

func TestRenderRoffManpage(t *testing.T) {
	if !roffRendererInstalled() {
		t.Skip("no supported roff-to-HTML renderer is installed")
	}

	roff := []byte(".Dd July 28, 2026\n.Dt PISCSI 1\n.Os PiSCSI\n" +
		".Sh NAME\n.Nm piscsi\n.Nd emulate SCSI devices\n")
	html, err := renderRoffManpage(context.Background(), roff)
	if err != nil {
		t.Fatalf("renderRoffManpage() error = %v", err)
	}
	if !strings.Contains(strings.ToLower(string(html)), "piscsi") {
		t.Errorf("renderRoffManpage() output does not contain piscsi: %s", html)
	}
}

func TestManpageLinksUseWebEndpoint(t *testing.T) {
	body := `<A HREF="/?1+scsictl">scsictl(1)</A>`
	got := manpageLinkPattern.ReplaceAllString(body, `href="/sys/manpage?app=$1"`)
	want := `<A href="/sys/manpage?app=scsictl">scsictl(1)</A>`
	if got != want {
		t.Errorf("rewritten manpage link = %q, want %q", got, want)
	}
}

func roffRendererInstalled() bool {
	for _, name := range []string{"mandoc", "man2html", "groff"} {
		if _, err := exec.LookPath(name); err == nil {
			return true
		}
	}
	return false
}
