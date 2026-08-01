// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

// Package oled implements the PiSCSI status monitor's display and rendering
// boundary. The package deliberately does not expose an image-library specific
// API, which makes both the renderer and the I2C transport straightforward to
// test without hardware.
package oled

import "fmt"

const Width = 128

// Frame is a row-major, most-significant-bit-first, one-bit framebuffer.
// A set bit is a lit pixel.
type Frame struct {
	Width  int
	Height int
	Pix    []byte
}

func NewFrame(width, height int) Frame {
	return Frame{Width: width, Height: height, Pix: make([]byte, ((width+7)/8)*height)}
}

func (f Frame) valid() bool {
	return f.Width == Width && (f.Height == 32 || f.Height == 64) && len(f.Pix) == ((f.Width+7)/8)*f.Height
}

func (f Frame) index(x, y int) (int, byte, error) {
	if !f.valid() || x < 0 || y < 0 || x >= f.Width || y >= f.Height {
		return 0, 0, fmt.Errorf("invalid framebuffer coordinate (%d, %d)", x, y)
	}
	return y*(f.Width/8) + x/8, 0x80 >> uint(x%8), nil
}

func (f Frame) At(x, y int) bool {
	i, bit, err := f.index(x, y)
	return err == nil && f.Pix[i]&bit != 0
}

func (f Frame) Set(x, y int, on bool) {
	i, bit, err := f.index(x, y)
	if err != nil {
		return
	}
	if on {
		f.Pix[i] |= bit
	} else {
		f.Pix[i] &^= bit
	}
}

// Display owns a physical panel.
type Display interface {
	Init() error
	Present(Frame) error
	Clear() error
	Close() error
}

// ssd1306Page converts the row-major renderer frame to SSD1306's vertical
// page layout and prefixes the I2C data control byte.
func ssd1306Page(frame Frame, page int) []byte {
	data := make([]byte, Width+1)
	data[0] = 0x40
	for x := 0; x < Width; x++ {
		for bit := 0; bit < 8; bit++ {
			if frame.At(x, page*8+bit) {
				data[x+1] |= 1 << uint(bit)
			}
		}
	}
	return data
}
