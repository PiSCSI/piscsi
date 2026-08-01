// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package oled

// PushFrame combines two same-sized frames into one horizontal push-transition
// frame. offset is the number of pixels the old frame has moved. When left is
// true, the old frame exits to the left and the new frame enters from the
// right; otherwise the direction is reversed.
func PushFrame(from, to Frame, offset int, left bool) Frame {
	if from.Width != to.Width || from.Height != to.Height || !from.valid() || !to.valid() {
		return NewFrame(from.Width, from.Height)
	}
	offset = max(0, min(offset, from.Width))
	out := NewFrame(from.Width, from.Height)
	for y := 0; y < out.Height; y++ {
		for x := 0; x < out.Width; x++ {
			var pixel bool
			if left {
				if x+offset < out.Width {
					pixel = from.At(x+offset, y)
				} else {
					pixel = to.At(x+offset-out.Width, y)
				}
			} else if x < offset {
				pixel = to.At(out.Width-offset+x, y)
			} else {
				pixel = from.At(x-offset, y)
			}
			out.Set(x, y, pixel)
		}
	}
	return out
}
