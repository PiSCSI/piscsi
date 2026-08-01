// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"fmt"
	"os"
	"sort"
	"strings"

	"github.com/piscsi/piscsi/go/piscsi/configuration"
)

// ProfileSelection identifies a validated configuration-file basename.
type ProfileSelection struct{ Filename string }

// NewProfileMenu reads only direct object-style configuration files from
// configDir. The selected document is fully validated later by Loader before
// any daemon mutation happens.
func NewProfileMenu(configDir string, pageSize int) (*Menu, error) {
	if strings.TrimSpace(configDir) == "" {
		return nil, fmt.Errorf("configuration directory is required")
	}
	entries, err := os.ReadDir(configDir)
	if err != nil {
		return nil, fmt.Errorf("read configuration directory: %w", err)
	}
	filenames := make([]string, 0, len(entries))
	for _, entry := range entries {
		if entry.IsDir() || !strings.EqualFold(filepathExt(entry.Name()), configuration.FileSuffix) {
			continue
		}
		if _, err := configuration.NormalizeFilename(entry.Name()); err == nil {
			filenames = append(filenames, entry.Name())
		}
	}
	sort.Slice(filenames, func(left, right int) bool {
		return strings.ToLower(filenames[left]) < strings.ToLower(filenames[right])
	})
	items := []MenuItem{{ID: "return", Label: "Return", Data: SlotAction{Kind: SlotActionReturn}}}
	for _, filename := range filenames {
		items = append(items, MenuItem{ID: "profile:" + filename, Label: filename, Data: ProfileSelection{Filename: filename}})
	}
	if len(items) == 1 {
		items = append(items, MenuItem{ID: "empty", Label: "(No profiles found)"})
	}
	return NewMenu("Load Profile", items, pageSize)
}

// filepathExt is kept narrow so this file has no path-manipulation behavior
// beyond filtering names. NormalizeFilename remains the security boundary.
func filepathExt(name string) string {
	index := strings.LastIndexByte(name, '.')
	if index < 0 {
		return ""
	}
	return name[index:]
}
