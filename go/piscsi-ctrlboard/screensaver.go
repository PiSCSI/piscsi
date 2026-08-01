// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"fmt"
	"time"

	"github.com/piscsi/piscsi/go/piscsi/hostinfo"
)

// IPScreenSaver shows local network status after a period without Control
// Board activity and moves it periodically to reduce OLED burn-in.
type IPScreenSaver struct {
	idleTimeout  time.Duration
	moveInterval time.Duration
	lastActivity time.Time
	nextMove     time.Time
	row          int
	active       bool
}

func NewIPScreenSaver(idleTimeout, moveInterval time.Duration) (*IPScreenSaver, error) {
	if idleTimeout <= 0 || moveInterval <= 0 {
		return nil, fmt.Errorf("screensaver durations must be positive")
	}
	return &IPScreenSaver{idleTimeout: idleTimeout, moveInterval: moveInterval, row: -1}, nil
}

func (s *IPScreenSaver) Reset(now time.Time) {
	s.lastActivity, s.nextMove = now, time.Time{}
	s.row, s.active = -1, false
}

// Update reports whether the saver is active and whether the frame needs a
// redraw. rows is the number of Control Board text rows.
func (s *IPScreenSaver) Update(now time.Time, rows int) (bool, bool) {
	if rows <= 0 || s.lastActivity.IsZero() {
		return false, false
	}
	if !s.active {
		if now.Sub(s.lastActivity) < s.idleTimeout {
			return false, false
		}
		s.active, s.row, s.nextMove = true, 0, now.Add(s.moveInterval)
		return true, true
	}
	if !now.Before(s.nextMove) {
		s.row = (s.row + 1) % rows
		s.nextMove = now.Add(s.moveInterval)
		return true, true
	}
	return true, false
}

func screenSaverLine() string {
	ip, hostname := hostinfo.Network()
	if ip == "" {
		return "No network"
	}
	return fmt.Sprintf("IP %s - %s", ip, hostname)
}
