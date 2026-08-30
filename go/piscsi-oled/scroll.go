package oled

// DefaultHorizontalScrollStep matches the six-pixel glyph advance of the
// embedded Type Writer font at the renderer's 8-pixel text size.
const DefaultHorizontalScrollStep = 6

// HorizontalScrollStartDelayFrames is the number of refreshes that show the
// starting parameter window before scrolling begins.
const HorizontalScrollStartDelayFrames = 3

// HorizontalScrollEndDelayFrames is the number of additional refreshes that
// show the final parameter window before wrapping to the start.
const HorizontalScrollEndDelayFrames = 1

// StatusLine separates the fixed SCSI identifier/type prefix from the
// parameter text. Only Parameter is eligible for horizontal scrolling.
type StatusLine struct {
	Fixed     string
	Parameter string
}

// Text returns the complete, unscrolled display text for a line.
func (l StatusLine) Text() string {
	if l.Fixed == "" || l.Parameter == "" {
		return l.Fixed + l.Parameter
	}
	return l.Fixed + " " + l.Parameter
}

// HorizontalScroller advances overflowing parameters in pixels from left to right.
// Each cycle holds the starting and final windows briefly, then returns to the
// start. Fixed prefixes always remain at column zero.
type HorizontalScroller struct {
	offsets     []int
	startFrames []int
	endFrames   []int
}

// Reset anchors all lines at their initial position.
func (s *HorizontalScroller) Reset(lineCount int) {
	s.offsets = make([]int, lineCount)
	s.startFrames = make([]int, lineCount)
	s.endFrames = make([]int, lineCount)
}

// Advance updates parameter offsets using a pixel step. maxOffsets contains
// the maximum pixel offset for each line. It returns a copy
// so callers can inspect the positions. A zero step pauses scrolling.
func (s *HorizontalScroller) Advance(maxOffsets []int, step int) []int {
	if len(s.offsets) != len(maxOffsets) {
		s.Reset(len(maxOffsets))
	}
	if step < 0 {
		step = 0
	}
	for i, maxOffset := range maxOffsets {
		if maxOffset <= 0 {
			s.offsets[i] = 0
			s.startFrames[i] = 0
			s.endFrames[i] = 0
			continue
		}
		if step == 0 {
			continue
		}
		if s.offsets[i] == 0 && s.startFrames[i] < HorizontalScrollStartDelayFrames {
			s.startFrames[i]++
			if s.startFrames[i] < HorizontalScrollStartDelayFrames {
				continue
			}
		}
		if s.offsets[i] >= maxOffset {
			if s.endFrames[i] < HorizontalScrollEndDelayFrames {
				s.endFrames[i]++
				continue
			}
			s.offsets[i] = 0
			s.startFrames[i] = 0
			s.endFrames[i] = 0
			continue
		}
		s.offsets[i] = min(s.offsets[i]+step, maxOffset)
		s.endFrames[i] = 0
	}
	return append([]int(nil), s.offsets...)
}

// Offsets returns the current pixel positions without advancing them.
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
	s.startFrames = append(s.startFrames[1:], s.startFrames[0])
	s.endFrames = append(s.endFrames[1:], s.endFrames[0])
}
