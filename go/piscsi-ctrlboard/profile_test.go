// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"os"
	"path/filepath"
	"testing"
)

func TestNewProfileMenuListsSortedJSONBasenames(t *testing.T) {
	directory := t.TempDir()
	for _, filename := range []string{"zeta.json", "Alpha.JSON", "ignore.txt", ".hidden.json"} {
		if err := os.WriteFile(filepath.Join(directory, filename), nil, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	menu, err := NewProfileMenu(directory, 4)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := len(menu.Items), 3; got != want {
		t.Fatalf("items = %d, want %d: %#v", got, want, menu.Items)
	}
	if got, want := menu.Items[1].Label, "Alpha.JSON"; got != want {
		t.Fatalf("first profile = %q, want %q", got, want)
	}
	if selected, ok := menu.Items[2].Data.(ProfileSelection); !ok || selected.Filename != "zeta.json" {
		t.Fatalf("profile selection = %#v", menu.Items[2].Data)
	}
}

func TestNewProfileMenuHasEmptyState(t *testing.T) {
	menu, err := NewProfileMenu(t.TempDir(), 4)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := menu.Items[1].Label, "(No profiles found)"; got != want {
		t.Fatalf("empty label = %q, want %q", got, want)
	}
}
