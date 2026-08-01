package oled

import "testing"

func TestPushFrameMovesFramesInBothDirections(t *testing.T) {
	from, leftTarget := NewFrame(Width, 32), NewFrame(Width, 32)
	from.Set(1, 0, true)
	leftTarget.Set(1, 0, true)

	left := PushFrame(from, leftTarget, 2, true)
	if !left.At(Width-1, 0) || left.At(1, 0) {
		t.Fatal("left transition pixels were not shifted as expected")
	}
	rightTarget := NewFrame(Width, 32)
	rightTarget.Set(Width-1, 0, true)
	right := PushFrame(from, rightTarget, 2, false)
	if !right.At(1, 0) || right.At(Width-1, 0) {
		t.Fatal("right transition pixels were not shifted as expected")
	}
}

func TestPushFrameEndsAtTarget(t *testing.T) {
	from, to := NewFrame(Width, 32), NewFrame(Width, 32)
	from.Set(0, 0, true)
	to.Set(Width-1, 31, true)
	for _, left := range []bool{true, false} {
		frame := PushFrame(from, to, Width, left)
		if !frame.At(Width-1, 31) || frame.At(0, 0) {
			t.Fatalf("direction %t did not end at the target frame", left)
		}
	}
}
