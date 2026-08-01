// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package oled

import (
	"fmt"
	"math/rand/v2"
	"time"
)

// IPScreenSaver shows the network-status line at a different text row after a
// period without a PiSCSI status change. It is driven by the monitor loop so
// polling continues while the screensaver is active.
type IPScreenSaver struct {
	idleTimeout  time.Duration
	moveInterval time.Duration
	lastActivity time.Time
	nextMove     time.Time
	row          int
	active       bool
}

func NewIPScreenSaver(idleTimeout, moveInterval time.Duration) (*IPScreenSaver, error) {
	if idleTimeout <= 0 {
		return nil, fmt.Errorf("screensaver idle timeout must be positive")
	}
	if moveInterval <= 0 {
		return nil, fmt.Errorf("screensaver move interval must be positive")
	}
	return &IPScreenSaver{idleTimeout: idleTimeout, moveInterval: moveInterval, row: -1}, nil
}

// Reset records meaningful PiSCSI activity and returns the monitor to its
// normal display. It must be called for the first successful status response.
func (s *IPScreenSaver) Reset(now time.Time) {
	s.lastActivity = now
	s.nextMove = time.Time{}
	s.active = false
	s.row = -1
}

// Update returns whether the screensaver is active and whether its frame needs
// to be redrawn. rows is the number of available 8-pixel text rows.
func (s *IPScreenSaver) Update(now time.Time, rows int) (bool, bool) {
	if rows <= 0 || s.lastActivity.IsZero() {
		return false, false
	}
	if !s.active {
		if now.Sub(s.lastActivity) < s.idleTimeout {
			return false, false
		}
		s.active = true
		s.row = nextRow(rows, -1)
		s.nextMove = now.Add(s.moveInterval)
		return true, true
	}
	if !now.Before(s.nextMove) {
		s.row = nextRow(rows, s.row)
		s.nextMove = now.Add(s.moveInterval)
		return true, true
	}
	return true, false
}

// Row returns the active text row. It is valid only while Update reports that
// the screensaver is active.
func (s *IPScreenSaver) Row() int { return s.row }

func nextRow(rows, current int) int {
	if rows <= 1 {
		return 0
	}
	if current < 0 || current >= rows {
		return rand.IntN(rows)
	}
	row := rand.IntN(rows - 1)
	if row >= current {
		row++
	}
	return row
}

// BlankScreenSaver clears the display after a period without a PiSCSI status
// change. It is driven by the monitor loop so polling continues while the
// display is blank.
type BlankScreenSaver struct {
	idleTimeout  time.Duration
	lastActivity time.Time
	active       bool
}

func NewBlankScreenSaver(idleTimeout time.Duration) (*BlankScreenSaver, error) {
	if idleTimeout <= 0 {
		return nil, fmt.Errorf("screensaver idle timeout must be positive")
	}
	return &BlankScreenSaver{idleTimeout: idleTimeout}, nil
}

// Reset records meaningful PiSCSI activity and returns the monitor to its
// normal display. It must be called for the first successful status response.
func (s *BlankScreenSaver) Reset(now time.Time) {
	s.lastActivity = now
	s.active = false
}

// Update returns whether the screensaver is active and whether the panel must
// be cleared. The panel is cleared only once per activation.
func (s *BlankScreenSaver) Update(now time.Time) (bool, bool) {
	if s.lastActivity.IsZero() {
		return false, false
	}
	if !s.active && now.Sub(s.lastActivity) >= s.idleTimeout {
		s.active = true
		return true, true
	}
	return s.active, false
}
