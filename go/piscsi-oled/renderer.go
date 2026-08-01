// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package oled

import (
	"fmt"
	"image"
	"image/color"
	"image/draw"

	"golang.org/x/image/font"
	"golang.org/x/image/font/opentype"
	"golang.org/x/image/math/fixed"

	"github.com/piscsi/piscsi/go/piscsi/assets"
)

const lineSpacing = 8

// Renderer composes status text and embedded splash graphics into a Frame.
type Renderer struct {
	height   int
	rotation int
	face     font.Face
}

func NewRenderer(height, rotation int) (*Renderer, error) {
	if height != 32 && height != 64 {
		return nil, fmt.Errorf("unsupported display height %d", height)
	}
	if rotation != 0 && rotation != 180 {
		return nil, fmt.Errorf("unsupported rotation %d", rotation)
	}
	ttf, err := opentype.Parse(assets.TypeWriterFont())
	if err != nil {
		return nil, fmt.Errorf("load embedded font: %w", err)
	}
	face, err := opentype.NewFace(ttf, &opentype.FaceOptions{Size: 8, DPI: 72, Hinting: font.HintingFull})
	if err != nil {
		return nil, fmt.Errorf("create font face: %w", err)
	}
	return &Renderer{height: height, rotation: rotation, face: face}, nil
}

func (r *Renderer) Close() error { return r.face.Close() }

func (r *Renderer) Render(lines []string) Frame {
	return r.RenderScrolled(lines, nil)
}

// RenderScrolled renders each line with its own horizontal pixel offset.
// Positive offsets move text towards the left, exposing the part of a long
// line that would otherwise be clipped at the right edge of the display.
func (r *Renderer) RenderScrolled(lines []string, offsets []int) Frame {
	canvas := image.NewGray(image.Rect(0, 0, Width, r.height))
	drawer := &font.Drawer{Dst: canvas, Src: image.NewUniform(color.White), Face: r.face}
	for i, line := range lines {
		if i*lineSpacing >= r.height {
			break
		}
		offset := 0
		if i < len(offsets) {
			offset = offsets[i]
		}
		drawer.Dot = fixedPoint(-offset, i*lineSpacing+7)
		drawer.DrawString(line)
	}
	return r.frame(canvas)
}

// TextWidth returns the rendered width of a line in pixels.
func (r *Renderer) TextWidth(line string) int {
	drawer := &font.Drawer{Face: r.face}
	return drawer.MeasureString(line).Round()
}

// RenderLineAt draws one line at the specified 8-pixel text row.
func (r *Renderer) RenderLineAt(line string, row int) Frame {
	canvas := image.NewGray(image.Rect(0, 0, Width, r.height))
	if row >= 0 && row*lineSpacing < r.height {
		drawer := &font.Drawer{Dst: canvas, Src: image.NewUniform(color.White), Face: r.face}
		drawer.Dot = fixedPoint(0, row*lineSpacing+7)
		drawer.DrawString(line)
	}
	return r.frame(canvas)
}

func (r *Renderer) Splash(start bool) (Frame, error) {
	img, err := assets.Splash(start, r.height)
	if err != nil {
		return Frame{}, err
	}
	canvas := image.NewGray(image.Rect(0, 0, Width, r.height))
	draw.Draw(canvas, canvas.Bounds(), img, img.Bounds().Min, draw.Src)
	return r.frame(canvas), nil
}

func (r *Renderer) frame(img *image.Gray) Frame {
	frame := NewFrame(Width, r.height)
	for y := 0; y < r.height; y++ {
		for x := 0; x < Width; x++ {
			if img.GrayAt(x, y).Y >= 128 {
				frame.Set(x, y, true)
			}
		}
	}
	if r.rotation == 180 {
		return rotate180(frame)
	}
	return frame
}

func rotate180(in Frame) Frame {
	out := NewFrame(in.Width, in.Height)
	for y := 0; y < in.Height; y++ {
		for x := 0; x < in.Width; x++ {
			out.Set(in.Width-1-x, in.Height-1-y, in.At(x, y))
		}
	}
	return out
}

// fixedPoint avoids exposing fixed.Int26_6 outside this renderer.
func fixedPoint(x, y int) fixed.Point26_6 { return fixed.P(x, y) }
