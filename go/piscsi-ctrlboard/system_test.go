// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import "testing"

func TestNewSystemInfoMenuFormatsAvailableFacts(t *testing.T) {
	menu, err := NewSystemInfoMenu(SCSISlot{ID: 1}, SystemInfo{
		Hostname: "PiSCSI", IP: "192.0.2.1", DiskFreeMiB: 512,
		MemoryFreeMiB: 256, MemoryTotalMiB: 1024, CPUCount: 4,
		LoadAverage: "0.42", Version: "25.1.0", Environment: "Raspberry Pi, Linux arm64",
	}, 4)
	if err != nil {
		t.Fatal(err)
	}
	want := []string{"Return", "[PiSCSI]", "IP: 192.0.2.1", "Disk: 512 MB free", "CPU: 4 cores load 0.42", "Mem: 256/1024 MB free", "PiSCSI v25.1.0", "Raspberry Pi, Linux arm64"}
	if len(menu.Items) != len(want) {
		t.Fatalf("items = %d, want %d", len(menu.Items), len(want))
	}
	for index, label := range want {
		if got := menu.Items[index].Label; got != label {
			t.Errorf("item %d = %q, want %q", index, got, label)
		}
	}
}

func TestFormatVersion(t *testing.T) {
	if got, want := formatVersion(nil), ""; got != want {
		t.Fatalf("nil version = %q, want %q", got, want)
	}
}
