// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"testing"

	pb "github.com/piscsi/piscsi/go/proto"
)

func TestNewDeviceInfoMenuUsesAttachedDeviceState(t *testing.T) {
	menu, err := NewDeviceInfoMenu(SCSISlot{ID: 2, Device: &pb.PbDevice{
		Id:         2,
		Unit:       1,
		Type:       pb.PbDeviceType_SCCD,
		File:       &pb.PbImageFile{Name: "System.iso", Size: 1234, ReadOnly: true},
		Params:     map[string]string{"foo": "bar", "file": "System.iso"},
		Vendor:     "PiSCSI",
		Product:    "CD-ROM",
		Revision:   "1.0",
		BlockSize:  2048,
		Properties: &pb.PbDeviceProperties{Removable: true},
		Status:     &pb.PbDeviceStatus{Removed: false},
	}}, 4)
	if err != nil {
		t.Fatal(err)
	}
	want := []string{"Return", "ID   : 2", "LUN  : 1", "File : System.iso", "Type : SCCD", "R/RW : Read-only", "Prms : file=System.iso", "Prms : foo=bar", "Vndr : PiSCSI", "Prdct: CD-ROM", "Rvisn: 1.0", "Blksz: 2048", "Imgsz: 1234", "Media: present"}
	if len(menu.Items) != len(want) {
		t.Fatalf("menu items = %d, want %d", len(menu.Items), len(want))
	}
	for index, label := range want {
		if got := menu.Items[index].Label; got != label {
			t.Errorf("item %d = %q, want %q", index, got, label)
		}
	}
}

func TestNewDeviceInfoMenuHandlesEmptySlot(t *testing.T) {
	menu, err := NewDeviceInfoMenu(SCSISlot{ID: 7}, 4)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := menu.Items[1].Label, "(No device attached)"; got != want {
		t.Fatalf("empty state = %q, want %q", got, want)
	}
}
