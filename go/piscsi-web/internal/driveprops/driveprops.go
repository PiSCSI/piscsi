// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package driveprops

import (
	"encoding/json"
	"fmt"
	"os"
)

// DriveProperty represents a single drive configuration from the drive_properties.json file
type DriveProperty struct {
	DeviceType  string  `json:"device_type"`
	Vendor      *string `json:"vendor"`
	Product     string  `json:"product"`
	Revision    *string `json:"revision"`
	BlockSize   any     `json:"block_size"` // can be int or string
	Size        *int64  `json:"size"`
	Name        string  `json:"name"`
	FileType    *string `json:"file_type"`
	Description string  `json:"description"`
	URL         string  `json:"url"`
}

// Properties holds all drive properties loaded from the JSON file
type Properties struct {
	drives []DriveProperty
}

// LoadProperties reads and parses the drive_properties.json file
func LoadProperties(path string) (*Properties, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to read drive properties file: %w", err)
	}

	var drives []DriveProperty
	if err := json.Unmarshal(data, &drives); err != nil {
		return nil, fmt.Errorf("failed to parse drive properties file: %w", err)
	}

	return &Properties{drives: drives}, nil
}

// GetByName returns the drive properties for a given drive name
func (p *Properties) GetByName(name string) (*DriveProperty, error) {
	for _, drive := range p.drives {
		if drive.Name == name {
			return &drive, nil
		}
	}
	return nil, fmt.Errorf("drive '%s' not found in properties database", name)
}

// GetAllDrives returns all drive properties
func (p *Properties) GetAllDrives() []DriveProperty {
	return p.drives
}

// GetBlockSizeInt returns the block size as an integer
func (d *DriveProperty) GetBlockSizeInt() int {
	if d.BlockSize == nil {
		return 0
	}

	switch v := d.BlockSize.(type) {
	case float64:
		return int(v)
	case int:
		return v
	case string:
		// Empty string means variable block size (tape drives)
		return 0
	default:
		return 0
	}
}
