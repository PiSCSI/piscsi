// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"testing"

	oled "github.com/piscsi/piscsi/go/piscsi-oled"
)

func TestSCSIRefresherCoalescesImmediateRequests(t *testing.T) {
	refresher := &SCSIRefresher{requests: make(chan struct{}, 1)}
	refresher.RequestRefresh()
	refresher.RequestRefresh()
	if got := len(refresher.requests); got != 1 {
		t.Fatalf("queued refreshes = %d, want 1", got)
	}
}

func TestMenuControllerRequestsInstalledRootRefresh(t *testing.T) {
	renderer, err := NewRenderer(0)
	if err != nil {
		t.Fatal(err)
	}
	defer renderer.Close()
	menu, err := NewMenu("root", nil, renderer.Rows())
	if err != nil {
		t.Fatal(err)
	}
	controller, err := NewMenuController(menu, renderer, &fakePresenter{frames: make(chan oled.Frame, 1)})
	if err != nil {
		t.Fatal(err)
	}
	called := 0
	controller.SetRootRefresh(func() { called++ })
	controller.RequestRootRefresh()
	if called != 1 {
		t.Fatalf("root refresh calls = %d, want 1", called)
	}
}
