package oled

import (
	"bytes"
	"testing"
)

func TestRendererProducesPackedFramesForBothHeights(t *testing.T) {
	for _, height := range []int{32, 64} {
		t.Run(string(rune(height)), func(t *testing.T) {
			renderer, err := NewRenderer(height, 0)
			if err != nil {
				t.Fatal(err)
			}
			defer renderer.Close()
			frame := renderer.Render([]string{"0 HD disk.hds", "IP 192.0.2.1 - pi"})
			if !frame.valid() {
				t.Fatalf("invalid frame: %#v", frame)
			}
			if len(frame.Pix) != 16*height {
				t.Errorf("packed framebuffer size = %d, want %d", len(frame.Pix), 16*height)
			}
			if bytes.Count(frame.Pix, []byte{0}) == len(frame.Pix) {
				t.Fatal("text rendering produced a blank frame")
			}
		})
	}
}

func TestRendererRotationIs180Degrees(t *testing.T) {
	normal, err := NewRenderer(32, 0)
	if err != nil {
		t.Fatal(err)
	}
	defer normal.Close()
	rotated, err := NewRenderer(32, 180)
	if err != nil {
		t.Fatal(err)
	}
	defer rotated.Close()
	a := normal.Render([]string{"PiSCSI"})
	b := rotated.Render([]string{"PiSCSI"})
	for y := 0; y < a.Height; y++ {
		for x := 0; x < a.Width; x++ {
			if a.At(x, y) != b.At(a.Width-1-x, a.Height-1-y) {
				t.Fatalf("pixel (%d,%d) was not rotated", x, y)
			}
		}
	}
}

func TestRendererRendersLineAtRequestedRow(t *testing.T) {
	renderer, err := NewRenderer(32, 0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	first := renderer.RenderLineAt("IP 192.0.2.1 - pi", 0)
	last := renderer.RenderLineAt("IP 192.0.2.1 - pi", 3)
	if !first.valid() || !last.valid() {
		t.Fatal("RenderLineAt() produced an invalid frame")
	}
	if bytes.Equal(first.Pix, last.Pix) {
		t.Fatal("RenderLineAt() rendered different rows identically")
	}
}

func TestRendererHorizontalScrollChangesVisiblePixels(t *testing.T) {
	renderer, err := NewRenderer(32, 0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	line := "a very long device filename that exceeds the display width"
	if renderer.TextWidth(line) <= Width {
		t.Fatalf("test line width = %d, want more than %d", renderer.TextWidth(line), Width)
	}
	first := renderer.RenderScrolled([]string{line}, []int{0})
	shifted := renderer.RenderScrolled([]string{line}, []int{1})
	if bytes.Equal(first.Pix, shifted.Pix) {
		t.Fatal("horizontal scroll offset did not change rendered pixels")
	}
}

func TestRendererEmbeddedFontUsesSixPixelGlyphAdvance(t *testing.T) {
	renderer, err := NewRenderer(32, 0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	if got := renderer.TextWidth("M"); got != DefaultHorizontalScrollStep {
		t.Fatalf("glyph advance = %d, want %d", got, DefaultHorizontalScrollStep)
	}
}

func TestSplashFrames(t *testing.T) {
	for _, height := range []int{32, 64} {
		renderer, err := NewRenderer(height, 0)
		if err != nil {
			t.Fatal(err)
		}
		start, err := renderer.Splash(true)
		if err != nil {
			t.Fatal(err)
		}
		stop, err := renderer.Splash(false)
		if err != nil {
			t.Fatal(err)
		}
		renderer.Close()
		if !start.valid() || !stop.valid() || bytes.Equal(start.Pix, stop.Pix) {
			t.Fatalf("invalid splash conversion for %d pixels", height)
		}
	}
}

func TestSSD1306PagePacking(t *testing.T) {
	frame := NewFrame(Width, 32)
	frame.Set(0, 0, true)
	frame.Set(1, 7, true)
	frame.Set(127, 8, true)
	page0 := ssd1306Page(frame, 0)
	if page0[0] != 0x40 || page0[1] != 0x01 || page0[2] != 0x80 {
		t.Fatalf("page 0 = %#v, want data prefix and vertical pixels", page0[:3])
	}
	page1 := ssd1306Page(frame, 1)
	if page1[128] != 0x01 {
		t.Fatalf("page 1 final column = %#x, want 0x01", page1[128])
	}
}
