// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"testing"

	pb "github.com/piscsi/piscsi/go/proto"
)

func TestDeviceTypeMenuUsesDaemonTypesAndRetainsSlot(t *testing.T) {
	slot := SCSISlot{ID: 6}
	menu, err := NewDeviceTypeMenu(slot, []*pb.PbDeviceTypeProperties{
		{Type: pb.PbDeviceType_SAHD},
		{Type: pb.PbDeviceType_SCLP, Properties: &pb.PbDeviceProperties{DefaultParams: map[string]string{"cmd": "custom"}}},
		{Type: pb.PbDeviceType_SCHD, Properties: &pb.PbDeviceProperties{SupportsFile: true}},
	}, 4)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := menu.Items[1].Label, "SCSI Hard Disk [SCHD]"; got != want {
		t.Fatalf("first device = %q, want %q", got, want)
	}
	if got, want := menu.Items[3].Label, "SASI Hard Disk [SAHD]"; got != want {
		t.Fatalf("last device = %q, want %q", got, want)
	}
	selection, ok := menu.Items[2].Data.(DeviceTypeSelection)
	if !ok || selection.Slot.ID != 6 || selection.Type != pb.PbDeviceType_SCLP {
		t.Fatalf("printer selection = %#v", menu.Items[2].Data)
	}
}

func TestNetworkTopologyMenuOnlyOffersReadySupportedProfiles(t *testing.T) {
	menu, err := NewNetworkTopologyMenu(SCSISlot{ID: 2}, []*pb.PbNetworkInterface{
		{Name: "wlan0", Up: true, SupportedMode: []string{"proxyarp", "ignored"}},
		{Name: "eth0", Up: true, SupportedMode: []string{"bridge"}},
		{Name: "eth1", Up: false, SupportedMode: []string{"bridge"}},
	}, 4)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := menu.Items[1].Label, "Wired bridge (eth0)"; got != want {
		t.Fatalf("first topology = %q, want %q", got, want)
	}
	if got, want := menu.Items[2].Label, "Wi-Fi proxy ARP (wlan0)"; got != want {
		t.Fatalf("second topology = %q, want %q", got, want)
	}
	selection, ok := menu.Items[1].Data.(NetworkTopologySelection)
	if !ok || selection.Mode != "bridge" || selection.Interface != "eth0" {
		t.Fatalf("topology selection = %#v", menu.Items[1].Data)
	}
}

func TestDeviceOptionMenuCopiesDefaultParams(t *testing.T) {
	defaults := map[string]string{"cmd": "original"}
	menu, err := NewDeviceOptionMenu(DeviceTypeSelection{
		Slot: SCSISlot{ID: 1}, Type: pb.PbDeviceType_SCLP,
		Properties: &pb.PbDeviceProperties{DefaultParams: defaults},
	}, 4)
	if err != nil {
		t.Fatal(err)
	}
	selection := menu.Items[1].Data.(DeviceAttachSelection)
	selection.Params["cmd"] = "changed"
	if got := defaults["cmd"]; got != "original" {
		t.Fatalf("source default params mutated to %q", got)
	}
}

func TestPrinterOptionShowsEffectiveDefaultCommand(t *testing.T) {
	menu, err := NewDeviceOptionMenu(DeviceTypeSelection{Type: pb.PbDeviceType_SCLP}, 4)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := menu.Items[1].Label, "Attach Printer (lp -oraw %f)"; got != want {
		t.Fatalf("printer option = %q, want %q", got, want)
	}
}

func TestAddAttachWithoutMediaOptionAddsOnlyRemovableChoice(t *testing.T) {
	menu, err := NewMenu("Select Image", []MenuItem{{ID: "return", Label: "Return"}, {ID: "image:disk", Label: "disk"}}, 4)
	if err != nil {
		t.Fatal(err)
	}
	AddAttachWithoutMediaOption(menu, DeviceTypeSelection{
		Slot: SCSISlot{ID: 5}, Type: pb.PbDeviceType_SCMO,
		Properties: &pb.PbDeviceProperties{Removable: true},
	})
	if got, want := menu.Items[1].Label, "Attach with no media"; got != want {
		t.Fatalf("naked attach option = %q, want %q", got, want)
	}
	selection, ok := menu.Items[1].Data.(DeviceAttachSelection)
	if !ok || selection.Slot.ID != 5 || selection.Type != pb.PbDeviceType_SCMO {
		t.Fatalf("naked attach selection = %#v", menu.Items[1].Data)
	}

	AddAttachWithoutMediaOption(menu, DeviceTypeSelection{Type: pb.PbDeviceType_SCHD})
	if got, want := len(menu.Items), 3; got != want {
		t.Fatalf("non-removable option count = %d, want %d", got, want)
	}
}
