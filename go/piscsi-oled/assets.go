// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package oled

import (
	"bytes"
	_ "embed"
	"fmt"
	"image"

	"golang.org/x/image/bmp"
	"golang.org/x/image/font/opentype"
)

var (
	//go:embed assets/type_writer.ttf
	typeWriter []byte
	//go:embed assets/splash_start_32.bmp
	start32 []byte
	//go:embed assets/splash_start_64.bmp
	start64 []byte
	//go:embed assets/splash_stop_32.bmp
	stop32 []byte
	//go:embed assets/splash_stop_64.bmp
	stop64 []byte
)

func loadFont() (*opentype.Font, error) {
	return opentype.Parse(typeWriter)
}

func splash(start bool, height int) (image.Image, error) {
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
