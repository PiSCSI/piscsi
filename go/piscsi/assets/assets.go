// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

// Package assets provides UI-neutral embedded assets shared by PiSCSI's Go
// display applications. It intentionally contains no renderer or display
// transport policy.
package assets

import (
	"bytes"
	_ "embed"
	"fmt"
	"image"

	"golang.org/x/image/bmp"
)

var (
	//go:embed type_writer.ttf
	typeWriter []byte
	//go:embed DejaVuSansMono-Bold.ttf
	dejaVuSansMonoBold []byte
	//go:embed splash_start_32.bmp
	start32 []byte
	//go:embed splash_start_64.bmp
	start64 []byte
	//go:embed splash_stop_32.bmp
	stop32 []byte
	//go:embed splash_stop_64.bmp
	stop64 []byte
)

// TypeWriterFont returns a copy of the embedded status-monitor font. Callers
// own the returned bytes and may safely retain or modify them.
func TypeWriterFont() []byte { return bytes.Clone(typeWriter) }

// DejaVuSansMonoBoldFont returns a copy of the Control Board's menu font.
// Its DejaVu Fonts License is distributed beside the embedded font.
func DejaVuSansMonoBoldFont() []byte { return bytes.Clone(dejaVuSansMonoBold) }

// Splash decodes a startup or shutdown bitmap for a supported display height.
// A fresh image is returned on every call so callers cannot modify shared data.
func Splash(start bool, height int) (image.Image, error) {
	var data []byte
	switch {
	case start && height == 32:
		data = start32
	case start && height == 64:
		data = start64
	case !start && height == 32:
		data = stop32
	case !start && height == 64:
		data = stop64
	default:
		return nil, fmt.Errorf("unsupported display height %d", height)
	}
	return bmp.Decode(bytes.NewReader(data))
}
