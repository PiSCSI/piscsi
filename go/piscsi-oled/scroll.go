package oled

// DefaultHorizontalScrollStep matches the six-pixel glyph advance of the
// embedded Type Writer font at the renderer's 8-pixel text size.
const DefaultHorizontalScrollStep = 6

// HorizontalScroller advances overflowing lines and reverses direction at
// either edge. Short lines remain anchored at x=0.
type HorizontalScroller struct {
	offsets   []int
	direction []int
}

// Reset anchors all lines at their initial position.
func (s *HorizontalScroller) Reset(lineCount int) {
	s.offsets = make([]int, lineCount)
	s.direction = make([]int, lineCount)
	for i := range s.direction {
		s.direction[i] = 1
	}
}

// Advance updates offsets using the supplied rendered line widths and pixel
// step. It returns a copy so callers can safely pass the result to the
// renderer. A zero step pauses scrolling.
func (s *HorizontalScroller) Advance(widths []int, step int) []int {
	if len(s.offsets) != len(widths) {
		s.Reset(len(widths))
	}
	if step < 0 {
		step = 0
	}
	for i, width := range widths {
		maxOffset := width - Width
		if maxOffset <= 0 {
			s.offsets[i] = 0
			continue
		}
		if s.direction[i] == 0 {
			s.direction[i] = 1
		}
		s.offsets[i] += s.direction[i] * step
		if s.offsets[i] >= maxOffset {
			s.offsets[i] = maxOffset
			s.direction[i] = -1
		} else if s.offsets[i] <= 0 {
			s.offsets[i] = 0
			s.direction[i] = 1
		}
	}
	return append([]int(nil), s.offsets...)
}

// Offsets returns the current positions without advancing them.
func (s *HorizontalScroller) Offsets() []int {
	return append([]int(nil), s.offsets...)
}

// Rotate keeps offsets associated with their lines when the vertical status
// window advances.
func (s *HorizontalScroller) Rotate() {
	if len(s.offsets) < 2 {
		return
	}
	s.offsets = append(s.offsets[1:], s.offsets[0])
	s.direction = append(s.direction[1:], s.direction[0])
}
