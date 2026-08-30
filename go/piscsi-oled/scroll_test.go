package oled

import "testing"

func TestHorizontalScrollerAnchorsShortLinesDelaysAndWrapsLongLines(t *testing.T) {
	scroller := HorizontalScroller{}
	scroller.Reset(2)
	maxOffsets := []int{0, 12}
	if got := scroller.Advance(maxOffsets, 6); got[0] != 0 || got[1] != 0 {
		t.Fatalf("after first frame = %v, want [0 0]", got)
	}
	if got := scroller.Advance(maxOffsets, 6); got[0] != 0 || got[1] != 0 {
		t.Fatalf("after second frame = %v, want [0 0]", got)
	}
	if got := scroller.Advance(maxOffsets, 6); got[0] != 0 || got[1] != 6 {
		t.Fatalf("after third frame = %v, want [0 6]", got)
	}
	if got := scroller.Advance(maxOffsets, 6); got[0] != 0 || got[1] != 12 {
		t.Fatalf("at final window = %v, want [0 12]", got)
	}
	if got := scroller.Advance(maxOffsets, 6); got[0] != 0 || got[1] != 12 {
		t.Fatalf("after first final frame = %v, want one extra final frame at [0 12]", got)
	}
	if got := scroller.Advance(maxOffsets, 6); got[0] != 0 || got[1] != 0 {
		t.Fatalf("after final hold = %v, want reset to [0 0]", got)
	}
}

func TestHorizontalScrollerUsesConfiguredStep(t *testing.T) {
	scroller := HorizontalScroller{}
	scroller.Reset(1)
	maxOffsets := []int{24}
	scroller.Advance(maxOffsets, 4)
	scroller.Advance(maxOffsets, 4)
	if got := scroller.Advance(maxOffsets, 4); got[0] != 4 {
		t.Fatalf("advance = %v, want [4]", got)
	}
	if got := scroller.Advance(maxOffsets, 0); got[0] != 4 {
		t.Fatalf("paused advance = %v, want [4]", got)
	}
}

func TestHorizontalScrollerOffsetsReturnsCopy(t *testing.T) {
	scroller := HorizontalScroller{}
	scroller.Reset(1)
	got := scroller.Offsets()
	got[0] = 1
	if scroller.Offsets()[0] != 0 {
		t.Fatal("Offsets() exposed the scroller's internal state")
	}
}

func TestStatusLineText(t *testing.T) {
	if got := (StatusLine{Fixed: "6 HD", Parameter: "disk.hds"}).Text(); got != "6 HD disk.hds" {
		t.Fatalf("Text() = %q", got)
	}
}
