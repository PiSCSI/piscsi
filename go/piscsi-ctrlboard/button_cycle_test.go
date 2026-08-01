// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"testing"
	"time"

	oled "github.com/piscsi/piscsi/go/piscsi-oled"
)

func TestButtonCycleStartsAtReturnThenAdvances(t *testing.T) {
	renderer, err := NewRenderer(0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	menu, err := NewMenu("root", nil, renderer.Rows())
	if err != nil {
		t.Fatal(err)
	}
	controller, err := NewMenuController(menu, renderer, &fakePresenter{frames: make(chan oled.Frame, 4)})
	if err != nil {
		t.Fatal(err)
	}
	cycler := &ButtonCycler{menu: controller, timeout: time.Hour}
	cycle := &buttonCycle{choices: []buttonCycleChoice{{label: "Return ->"}, {label: "one"}, {label: "two"}}}
	cycler.advanceLocked(cycle, func(uint64) {})
	if cycle.index != 0 || cycle.generation != 1 {
		t.Fatalf("first choice index/generation = %d/%d", cycle.index, cycle.generation)
	}
	cycler.advanceLocked(cycle, func(uint64) {})
	if cycle.index != 1 || cycle.generation != 2 {
		t.Fatalf("second choice index/generation = %d/%d", cycle.index, cycle.generation)
	}
	cycler.advanceLocked(cycle, func(uint64) {})
	if cycle.index != 2 {
		t.Fatalf("third choice index = %d", cycle.index)
	}
}
