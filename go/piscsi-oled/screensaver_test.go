package oled

import (
	"testing"
	"time"
)

func TestIPScreenSaverActivationAndMovement(t *testing.T) {
	saver, err := NewIPScreenSaver(time.Minute, 30*time.Second)
	if err != nil {
		t.Fatal(err)
	}
	now := time.Date(2026, 8, 1, 12, 0, 0, 0, time.UTC)
	saver.Reset(now)
	if active, redraw := saver.Update(now.Add(time.Minute-time.Nanosecond), 4); active || redraw {
		t.Fatalf("Update() before timeout = active %t, redraw %t, want false, false", active, redraw)
	}
	if active, redraw := saver.Update(now.Add(time.Minute), 4); !active || !redraw {
		t.Fatalf("Update() at timeout = active %t, redraw %t, want true, true", active, redraw)
	}
	firstRow := saver.Row()
	if firstRow < 0 || firstRow >= 4 {
		t.Fatalf("Row() = %d, want a valid row", firstRow)
	}
	if active, redraw := saver.Update(now.Add(time.Minute+30*time.Second-time.Nanosecond), 4); !active || redraw {
		t.Fatalf("Update() before move = active %t, redraw %t, want true, false", active, redraw)
	}
	if active, redraw := saver.Update(now.Add(time.Minute+30*time.Second), 4); !active || !redraw {
		t.Fatalf("Update() at move = active %t, redraw %t, want true, true", active, redraw)
	}
	if saver.Row() == firstRow {
		t.Fatalf("Row() = %d, want a row other than %d", saver.Row(), firstRow)
	}
}

func TestIPScreenSaverResetAndValidation(t *testing.T) {
	for _, durations := range [][2]time.Duration{{0, time.Second}, {time.Second, 0}} {
		if _, err := NewIPScreenSaver(durations[0], durations[1]); err == nil {
			t.Fatalf("NewIPScreenSaver(%s, %s) error = nil", durations[0], durations[1])
		}
	}
	saver, err := NewIPScreenSaver(time.Second, time.Second)
	if err != nil {
		t.Fatal(err)
	}
	now := time.Now()
	saver.Reset(now)
	saver.Update(now.Add(time.Second), 4)
	saver.Reset(now.Add(2 * time.Second))
	if active, redraw := saver.Update(now.Add(2*time.Second), 4); active || redraw {
		t.Fatalf("Update() after reset = active %t, redraw %t, want false, false", active, redraw)
	}
}

func TestBlankScreenSaverActivationAndReset(t *testing.T) {
	if _, err := NewBlankScreenSaver(0); err == nil {
		t.Fatal("NewBlankScreenSaver(0) error = nil")
	}
	saver, err := NewBlankScreenSaver(time.Minute)
	if err != nil {
		t.Fatal(err)
	}
	now := time.Date(2026, 8, 1, 12, 0, 0, 0, time.UTC)
	saver.Reset(now)
	if active, clear := saver.Update(now.Add(time.Minute - time.Nanosecond)); active || clear {
		t.Fatalf("Update() before timeout = active %t, clear %t, want false, false", active, clear)
	}
	if active, clear := saver.Update(now.Add(time.Minute)); !active || !clear {
		t.Fatalf("Update() at timeout = active %t, clear %t, want true, true", active, clear)
	}
	if active, clear := saver.Update(now.Add(2 * time.Minute)); !active || clear {
		t.Fatalf("Update() after activation = active %t, clear %t, want true, false", active, clear)
	}
	saver.Reset(now.Add(2 * time.Minute))
	if active, clear := saver.Update(now.Add(2 * time.Minute)); active || clear {
		t.Fatalf("Update() after reset = active %t, clear %t, want false, false", active, clear)
	}
}
