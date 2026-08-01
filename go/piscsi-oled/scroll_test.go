package oled

import "testing"

func TestHorizontalScrollerAnchorsShortLinesAndBouncesLongLines(t *testing.T) {
	scroller := HorizontalScroller{}
	scroller.Reset(2)
	widths := []int{Width - 1, Width + 2}
	if got := scroller.Advance(widths, 1); got[0] != 0 || got[1] != 1 {
		t.Fatalf("first advance = %v, want [0 1]", got)
	}
	if got := scroller.Advance([]int{Width - 1, Width + 2}, 1); got[0] != 0 || got[1] != 2 {
		t.Fatalf("second advance = %v, want [0 2]", got)
	}
	if got := scroller.Advance([]int{Width - 1, Width + 2}, 1); got[1] != 1 {
		t.Fatalf("third advance = %v, want second line to reverse", got)
	}
}

func TestHorizontalScrollerUsesConfiguredStep(t *testing.T) {
	scroller := HorizontalScroller{}
	scroller.Reset(1)
	if got := scroller.Advance([]int{Width + 10}, 4); got[0] != 4 {
		t.Fatalf("advance = %v, want [4]", got)
	}
	if got := scroller.Advance([]int{Width + 10}, 0); got[0] != 4 {
		t.Fatalf("paused advance = %v, want [4]", got)
	}
}
