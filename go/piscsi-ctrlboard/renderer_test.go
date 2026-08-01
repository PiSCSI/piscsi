package ctrlboard

import (
	"bytes"
	"testing"
)

func TestRendererInvertsSelectedRow(t *testing.T) {
	renderer, err := NewRenderer(0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	menu := testMenu(t, 2, renderer.Rows())
	frame := renderer.Render(menu, 0)
	if !frame.At(DisplayWidth-1, 0) {
		t.Fatal("selected row background is not inverted")
	}
	if frame.At(DisplayWidth-1, renderer.lineHeight) {
		t.Fatal("unselected row background was inverted")
	}
}

func TestRendererScrollAndRotation(t *testing.T) {
	normal, err := NewRenderer(0)
	if err != nil {
		t.Fatal(err)
	}
	defer normal.Close()
	rotated, err := NewRenderer(180)
	if err != nil {
		t.Fatal(err)
	}
	defer rotated.Close()
	menu, err := NewMenu("test", []MenuItem{{ID: "long", Label: "a deliberately long menu entry that exceeds the display width"}}, normal.Rows())
	if err != nil {
		t.Fatal(err)
	}
	if normal.TextWidth(menu.Items[0].Label) <= DisplayWidth {
		t.Fatal("test label is not long enough")
	}
	a, shifted := normal.Render(menu, 0), normal.Render(menu, 2)
	if bytes.Equal(a.Pix, shifted.Pix) {
		t.Fatal("selected scroll offset did not change pixels")
	}
	b := rotated.Render(menu, 0)
	for y := 0; y < a.Height; y++ {
		for x := 0; x < a.Width; x++ {
			if a.At(x, y) != b.At(a.Width-1-x, a.Height-1-y) {
				t.Fatalf("pixel (%d,%d) was not rotated", x, y)
			}
		}
	}
}

func TestRendererSplashUsesDisplayDimensions(t *testing.T) {
	renderer, err := NewRenderer(0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	frame, err := renderer.Splash(true)
	if err != nil {
		t.Fatal(err)
	}
	if frame.Width != DisplayWidth || frame.Height != DisplayHeight {
		t.Fatalf("splash dimensions = %dx%d", frame.Width, frame.Height)
	}
	if bytes.Equal(frame.Pix, make([]byte, len(frame.Pix))) {
		t.Fatal("startup splash is blank")
	}
}
