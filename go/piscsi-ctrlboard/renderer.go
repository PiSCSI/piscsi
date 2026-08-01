// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"fmt"
	"image"
	"image/color"
	"image/draw"

	oled "github.com/piscsi/piscsi/go/piscsi-oled"
	"github.com/piscsi/piscsi/go/piscsi/assets"
	"golang.org/x/image/font"
	"golang.org/x/image/font/opentype"
	"golang.org/x/image/math/fixed"
)

const (
	DisplayWidth  = oled.Width
	DisplayHeight = 64
	menuFontSize  = 12
	scrollGap     = 16
)

// Renderer draws the Control Board's 128x64 menu contract. It owns the
// DejaVu face but not a physical display, keeping golden tests hardware-free.
type Renderer struct {
	rotation   int
	face       font.Face
	lineHeight int
	ascent     int
}

func NewRenderer(rotation int) (*Renderer, error) {
	if rotation != 0 && rotation != 180 {
		return nil, fmt.Errorf("unsupported rotation %d", rotation)
	}
	ttf, err := opentype.Parse(assets.DejaVuSansMonoBoldFont())
	if err != nil {
		return nil, fmt.Errorf("load embedded DejaVu font: %w", err)
	}
	face, err := opentype.NewFace(ttf, &opentype.FaceOptions{Size: menuFontSize, DPI: 72, Hinting: font.HintingFull})
	if err != nil {
		return nil, fmt.Errorf("create menu font face: %w", err)
	}
	metrics := face.Metrics()
	lineHeight := metrics.Height.Ceil()
	if lineHeight <= 0 || DisplayHeight/lineHeight == 0 {
		face.Close()
		return nil, fmt.Errorf("menu font does not fit the display")
	}
	return &Renderer{rotation: rotation, face: face, lineHeight: lineHeight, ascent: metrics.Ascent.Ceil()}, nil
}

func (r *Renderer) Close() error { return r.face.Close() }
func (r *Renderer) Rows() int    { return DisplayHeight / r.lineHeight }

func (r *Renderer) TextWidth(text string) int {
	return (&font.Drawer{Face: r.face}).MeasureString(text).Round()
}

// Render draws the menu's current page. selectedOffset moves only the selected
// label left, allowing the application scheduler to implement non-blocking
// horizontal scrolling without changing menu state.
func (r *Renderer) Render(menu *Menu, selectedOffset int) oled.Frame {
	canvas := image.NewGray(image.Rect(0, 0, DisplayWidth, DisplayHeight))
	if menu == nil {
		return r.toFrame(canvas)
	}
	drawer := &font.Drawer{Dst: canvas, Face: r.face}
	for row, item := range menu.Visible() {
		index := menu.firstRow + row
		top := row * r.lineHeight
		selected := index == menu.selected
		if selected {
			draw.Draw(canvas, image.Rect(0, top, DisplayWidth, min(top+r.lineHeight, DisplayHeight)), image.NewUniform(color.White), image.Point{}, draw.Src)
			drawer.Src = image.NewUniform(color.Black)
		} else {
			drawer.Src = image.NewUniform(color.White)
		}
		x := 0
		if selected {
			x = -max(selectedOffset, 0)
		}
		drawer.Dot = fixed.P(x, top+r.ascent)
		drawer.DrawString(item.Label)
		if selected && selectedOffset > 0 && r.TextWidth(item.Label) > DisplayWidth {
			// Draw a second copy after a short blank gap. This makes the end-to-
			// start transition continuous rather than leaving a blank screen.
			drawer.Dot = fixed.P(x+r.TextWidth(item.Label)+scrollGap, top+r.ascent)
			drawer.DrawString(item.Label)
		}
	}
	return r.toFrame(canvas)
}

// ScrollPeriod returns the number of one-pixel positions in one continuous
// selected-row scroll cycle, or zero for labels that already fit.
func (r *Renderer) ScrollPeriod(label string) int {
	width := r.TextWidth(label)
	if width <= DisplayWidth {
		return 0
	}
	return width + scrollGap
}

// RenderMessage draws a centered full-screen message over the menu.
func (r *Renderer) RenderMessage(message string) oled.Frame {
	canvas := image.NewGray(image.Rect(0, 0, DisplayWidth, DisplayHeight))
	drawer := &font.Drawer{Dst: canvas, Src: image.NewUniform(color.White), Face: r.face}
	width := drawer.MeasureString(message).Round()
	drawer.Dot = fixed.P((DisplayWidth-width)/2, (DisplayHeight-r.lineHeight)/2+r.ascent)
	drawer.DrawString(message)
	return r.toFrame(canvas)
}

// RenderScreenSaver draws a single unselected status line at row. Callers
// provide a valid row from IPScreenSaver.
func (r *Renderer) RenderScreenSaver(text string, row int) oled.Frame {
	canvas := image.NewGray(image.Rect(0, 0, DisplayWidth, DisplayHeight))
	if row >= 0 && row*r.lineHeight < DisplayHeight {
		drawer := &font.Drawer{Dst: canvas, Src: image.NewUniform(color.White), Face: r.face}
		drawer.Dot = fixed.P(0, row*r.lineHeight+r.ascent)
		drawer.DrawString(text)
	}
	return r.toFrame(canvas)
}

// Splash renders the shared Control Board startup or shutdown artwork using
// the same rotation path as menu frames.
func (r *Renderer) Splash(start bool) (oled.Frame, error) {
	img, err := assets.Splash(start, DisplayHeight)
	if err != nil {
		return oled.Frame{}, err
	}
	canvas := image.NewGray(image.Rect(0, 0, DisplayWidth, DisplayHeight))
	draw.Draw(canvas, canvas.Bounds(), img, img.Bounds().Min, draw.Src)
	return r.toFrame(canvas), nil
}

func (r *Renderer) toFrame(canvas *image.Gray) oled.Frame {
	frame := oled.NewFrame(DisplayWidth, DisplayHeight)
	for y := 0; y < DisplayHeight; y++ {
		for x := 0; x < DisplayWidth; x++ {
			frame.Set(x, y, canvas.GrayAt(x, y).Y >= 128)
		}
	}
	if r.rotation == 180 {
		return rotateFrame180(frame)
	}
	return frame
}

func rotateFrame180(in oled.Frame) oled.Frame {
	out := oled.NewFrame(in.Width, in.Height)
	for y := 0; y < in.Height; y++ {
		for x := 0; x < in.Width; x++ {
			out.Set(in.Width-1-x, in.Height-1-y, in.At(x, y))
		}
	}
	return out
}
