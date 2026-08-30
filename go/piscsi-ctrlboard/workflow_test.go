// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"context"
	"testing"

	pb "github.com/piscsi/piscsi/go/proto"
)

type workflowClient struct {
	commands []*pb.PbCommand
	results  []*pb.PbResult
}

func (c *workflowClient) SendCommand(command *pb.PbCommand) (*pb.PbResult, error) {
	c.commands = append(c.commands, command)
	result := c.results[0]
	c.results = c.results[1:]
	return result, nil
}

func TestBuildImageMenuSortsImagesAndCarriesSlot(t *testing.T) {
	client := &workflowClient{results: []*pb.PbResult{
		{Status: true, Result: &pb.PbResult_ImageFilesInfo{ImageFilesInfo: &pb.PbImageFilesInfo{ImageFiles: []*pb.PbImageFile{
			{Name: "zeta.hds", Type: pb.PbDeviceType_SCHD},
			{Name: "alpha.hda", Type: pb.PbDeviceType_SCHD},
		}}}},
	}}
	workflow := NewSCSIWorkflow(client, "secret")
	diagnostics := make(chan ImageListDiagnostic, 1)
	workflow.SetDiagnosticSink(func(response ImageListDiagnostic) { diagnostics <- response })
	slot := SCSISlot{ID: 3}
	menu, err := workflow.BuildImageMenu(context.Background(), slot, 4)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := menu.Items[1].Label, "alpha.hda [SCHD]"; got != want {
		t.Fatalf("first image = %q, want %q", got, want)
	}
	selection, ok := menu.Items[1].Data.(ImageSelection)
	if !ok || selection.Slot.ID != 3 || selection.Image.GetName() != "alpha.hda" {
		t.Fatalf("image selection = %#v", menu.Items[1].Data)
	}
	if _, found := client.commands[0].GetParams()["folder"]; found {
		t.Fatalf("default image request unexpectedly has a folder parameter")
	}
	if diagnostic := <-diagnostics; !diagnostic.Status || diagnostic.Count != 2 {
		t.Fatalf("image list diagnostic = %#v", diagnostic)
	}
}

func TestBuildImageMenuIncludesEmptyState(t *testing.T) {
	workflow := NewSCSIWorkflow(&workflowClient{results: []*pb.PbResult{{
		Status: true, Result: &pb.PbResult_ImageFilesInfo{ImageFilesInfo: &pb.PbImageFilesInfo{}},
	}}}, "")
	menu, err := workflow.BuildImageMenu(context.Background(), SCSISlot{ID: 1}, 4)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := menu.Items[1].Label, "(No image files found)"; got != want {
		t.Fatalf("empty label = %q, want %q", got, want)
	}
}

func TestBuildImageMenuForTypeFiltersDaemonMappedImages(t *testing.T) {
	workflow := NewSCSIWorkflow(&workflowClient{results: []*pb.PbResult{{
		Status: true, Result: &pb.PbResult_ImageFilesInfo{ImageFilesInfo: &pb.PbImageFilesInfo{ImageFiles: []*pb.PbImageFile{
			{Name: "disk.hds", Type: pb.PbDeviceType_SCHD},
			{Name: "media.mos", Type: pb.PbDeviceType_SCMO},
		}}},
	}}}, "")
	menu, err := workflow.BuildImageMenuForType(context.Background(), SCSISlot{ID: 1}, pb.PbDeviceType_SCMO, 4)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := len(menu.Items), 2; got != want {
		t.Fatalf("image item count = %d, want %d", got, want)
	}
	if got, want := menu.Items[1].Label, "media.mos [SCMO]"; got != want {
		t.Fatalf("filtered image = %q, want %q", got, want)
	}
}

func TestAttachOrInsertUsesInsertForEmptyRemovableDevice(t *testing.T) {
	client := &workflowClient{results: []*pb.PbResult{{Status: true}}}
	device := &pb.PbDevice{
		Id:         4,
		Unit:       1,
		Type:       pb.PbDeviceType_SCCD,
		Properties: &pb.PbDeviceProperties{Removable: true},
		Status:     &pb.PbDeviceStatus{Removed: true},
	}
	message, err := NewSCSIWorkflow(client, "").AttachOrInsert(context.Background(), ImageSelection{
		Slot:  SCSISlot{ID: 4, Device: device},
		Image: &pb.PbImageFile{Name: "System.iso", Type: pb.PbDeviceType_SCCD},
	})
	if err != nil {
		t.Fatal(err)
	}
	if got, want := message, "Inserted ID 4"; got != want {
		t.Fatalf("message = %q, want %q", got, want)
	}
	command := client.commands[0]
	if got, want := command.GetOperation(), pb.PbOperation_INSERT; got != want {
		t.Fatalf("operation = %s, want %s", got, want)
	}
	if got, want := command.GetDevices()[0].GetUnit(), int32(1); got != want {
		t.Fatalf("LUN = %d, want %d", got, want)
	}
}

func TestAttachDeviceUsesPrinterDefaultCommand(t *testing.T) {
	client := &workflowClient{results: []*pb.PbResult{{Status: true}}}
	message, err := NewSCSIWorkflow(client, "").AttachDevice(context.Background(), DeviceAttachSelection{
		Slot: SCSISlot{ID: 3}, Type: pb.PbDeviceType_SCLP, Params: map[string]string{"cmd": "different"},
	})
	if err != nil {
		t.Fatal(err)
	}
	if got, want := message, "Attached ID 3"; got != want {
		t.Fatalf("message = %q, want %q", got, want)
	}
	device := client.commands[0].GetDevices()[0]
	if got, want := device.GetParams()["cmd"], defaultPrinterCommand; got != want {
		t.Fatalf("printer command = %q, want %q", got, want)
	}
}

func TestDetachOrEjectEjectsPresentRemovableMedia(t *testing.T) {
	client := &workflowClient{results: []*pb.PbResult{{Status: true}}}
	message, err := NewSCSIWorkflow(client, "").DetachOrEject(context.Background(), SCSISlot{
		ID: 5,
		Device: &pb.PbDevice{
			Id:         5,
			Unit:       0,
			Properties: &pb.PbDeviceProperties{Removable: true},
			Status:     &pb.PbDeviceStatus{Removed: false},
			File:       &pb.PbImageFile{Name: "disk.mo"},
		},
	})
	if err != nil {
		t.Fatal(err)
	}
	if got, want := message, "Ejected ID 5"; got != want {
		t.Fatalf("message = %q, want %q", got, want)
	}
	if got, want := client.commands[0].GetOperation(), pb.PbOperation_EJECT; got != want {
		t.Fatalf("operation = %s, want %s", got, want)
	}
}

func TestReservePreservesExistingReservations(t *testing.T) {
	client := &workflowClient{results: []*pb.PbResult{
		{Status: true, Result: &pb.PbResult_ReservedIdsInfo{ReservedIdsInfo: &pb.PbReservedIdsInfo{Ids: []int32{1, 6}}}},
		{Status: true},
	}}
	message, err := NewSCSIWorkflow(client, "").Reserve(context.Background(), SCSISlot{ID: 4})
	if err != nil {
		t.Fatal(err)
	}
	if got, want := message, "Reserved ID 4"; got != want {
		t.Fatalf("message = %q, want %q", got, want)
	}
	if got, want := client.commands[0].GetOperation(), pb.PbOperation_RESERVED_IDS_INFO; got != want {
		t.Fatalf("lookup operation = %s, want %s", got, want)
	}
	if got, want := client.commands[1].GetParams()["ids"], "1,6,4"; got != want {
		t.Fatalf("reserved IDs = %q, want %q", got, want)
	}
}

func TestReleasePreservesOtherReservations(t *testing.T) {
	client := &workflowClient{results: []*pb.PbResult{
		{Status: true, Result: &pb.PbResult_ReservedIdsInfo{ReservedIdsInfo: &pb.PbReservedIdsInfo{Ids: []int32{1, 4, 6}}}},
		{Status: true},
	}}
	message, err := NewSCSIWorkflow(client, "").Release(context.Background(), SCSISlot{ID: 4, Reserved: true})
	if err != nil {
		t.Fatal(err)
	}
	if got, want := message, "Released ID 4"; got != want {
		t.Fatalf("message = %q, want %q", got, want)
	}
	if got, want := client.commands[1].GetParams()["ids"], "1,6"; got != want {
		t.Fatalf("reserved IDs = %q, want %q", got, want)
	}
}
