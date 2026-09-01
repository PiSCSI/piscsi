package server

import (
	"errors"
	"os"
	"path/filepath"
	"testing"
)

func TestResolvePathWithin(t *testing.T) {
	root := t.TempDir()
	got, err := resolvePathWithin(root, "subdir/disk.hds")
	if err != nil {
		t.Fatalf("resolvePathWithin() error = %v", err)
	}
	want := filepath.Join(root, "subdir", "disk.hds")
	if got != want {
		t.Fatalf("resolvePathWithin() = %q, want %q", got, want)
	}
}

func TestResolvePathWithinRejectsUnsafePaths(t *testing.T) {
	root := t.TempDir()
	for _, name := range []string{"", ".", "../disk.hds", "subdir/../../disk.hds", "/tmp/disk.hds"} {
		t.Run(name, func(t *testing.T) {
			if _, err := resolvePathWithin(root, name); err == nil {
				t.Fatalf("resolvePathWithin(%q) accepted an unsafe path", name)
			}
		})
	}
}

func TestResolveImagePath(t *testing.T) {
	root := t.TempDir()
	want := filepath.Join(root, "subdir", "disk.hds")

	for _, name := range []string{"subdir/disk.hds", want} {
		got, err := resolveImagePath(root, name)
		if err != nil {
			t.Fatalf("resolveImagePath(%q) error = %v", name, err)
		}
		if got != want {
			t.Fatalf("resolveImagePath(%q) = %q, want %q", name, got, want)
		}
	}

	if _, err := resolveImagePath(root, filepath.Join(filepath.Dir(root), "disk.hds")); err == nil {
		t.Fatal("resolveImagePath() accepted an absolute path outside the image directory")
	}
}

func TestUploadDestinationPathAllowsRoot(t *testing.T) {
	root := t.TempDir()
	got, err := uploadDestinationPath(root, "")
	if err != nil {
		t.Fatalf("uploadDestinationPath() error = %v", err)
	}
	if got != root {
		t.Fatalf("uploadDestinationPath() = %q, want %q", got, root)
	}
}

func TestUploadDestinationPathRejectsEscapingSymlink(t *testing.T) {
	root := t.TempDir()
	outside := t.TempDir()
	link := filepath.Join(root, "outside-link")
	if err := os.Symlink(outside, link); err != nil {
		t.Skipf("create symlink: %v", err)
	}

	if _, err := uploadDestinationPath(root, "outside-link"); err == nil {
		t.Fatal("uploadDestinationPath() accepted a symlink outside the configured directory")
	}
}

func TestOpenRegularFileWithinRejectsSymlink(t *testing.T) {
	root := t.TempDir()
	outside := filepath.Join(t.TempDir(), "secret.json")
	if err := os.WriteFile(outside, []byte("secret"), 0o600); err != nil {
		t.Fatal(err)
	}
	link := filepath.Join(root, "download.json")
	if err := os.Symlink(outside, link); err != nil {
		t.Skipf("create symbolic link: %v", err)
	}

	file, _, err := openRegularFileWithin(root, "download.json")
	if file != nil {
		file.Close()
		t.Fatal("openRegularFileWithin() opened a symbolic link")
	}
	if !errors.Is(err, errNotRegularFile) {
		t.Fatalf("openRegularFileWithin() error = %v, want errNotRegularFile", err)
	}
}
