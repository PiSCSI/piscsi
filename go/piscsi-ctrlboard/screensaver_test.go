// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"testing"
	"time"
)

func TestIPScreenSaverActivatesMovesAndResets(t *testing.T) {
	saver, err := NewIPScreenSaver(time.Minute, 30*time.Second)
	if err != nil {
		t.Fatal(err)
	}
	now := time.Date(2026, 8, 1, 12, 0, 0, 0, time.UTC)
	saver.Reset(now)
	if active, redraw := saver.Update(now.Add(time.Minute-time.Nanosecond), 4); active || redraw {
		t.Fatalf("before timeout = %t/%t", active, redraw)
	}
	if active, redraw := saver.Update(now.Add(time.Minute), 4); !active || !redraw || saver.row != 0 {
		t.Fatalf("at timeout = %t/%t row=%d", active, redraw, saver.row)
	}
	if active, redraw := saver.Update(now.Add(time.Minute+30*time.Second), 4); !active || !redraw || saver.row != 1 {
		t.Fatalf("at move = %t/%t row=%d", active, redraw, saver.row)
	}
	saver.Reset(now.Add(2 * time.Minute))
	if active, _ := saver.Update(now.Add(2*time.Minute), 4); active {
		t.Fatal("screensaver remained active after reset")
	}
}
