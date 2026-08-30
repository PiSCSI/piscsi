package ctrlboard

import (
	"context"
	"testing"

	pb "github.com/piscsi/piscsi/go/proto"
)

type menuClient struct {
	result *pb.PbResult
	err    error
}

func (c menuClient) SendCommand(*pb.PbCommand) (*pb.PbResult, error) { return c.result, c.err }

func TestSCSIMenuBuilderLabelsReservedAndAttachedSlots(t *testing.T) {
	builder := NewSCSIMenuBuilder(menuClient{result: &pb.PbResult{Status: true, Result: &pb.PbResult_ServerInfo{ServerInfo: &pb.PbServerInfo{
		ReservedIdsInfo: &pb.PbReservedIdsInfo{Ids: []int32{1}},
		DevicesInfo:     &pb.PbDevicesInfo{Devices: []*pb.PbDevice{{Id: 2, Unit: 0, Type: pb.PbDeviceType_SCHD, File: &pb.PbImageFile{Name: "/images/System.hds"}}}},
	}}}}, "")
	items, err := builder.Build(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	if got, want := items[1].Label, "1: [Reserved]"; got != want {
		t.Fatalf("slot 1 = %q, want %q", got, want)
	}
	if got, want := items[2].Label, "2: System.hds [SCHD]"; got != want {
		t.Fatalf("slot 2 = %q, want %q", got, want)
	}
	if got, want := items[3].Label, "3: (empty)"; got != want {
		t.Fatalf("slot 3 = %q, want %q", got, want)
	}
}

func TestSlotLabelRecognizesBothNetworkAdapters(t *testing.T) {
	for deviceType, want := range map[pb.PbDeviceType]string{
		pb.PbDeviceType_SCBR: "2: Host Bridge",
		pb.PbDeviceType_SCDP: "2: Daynaport",
	} {
		if got := slotLabel(SCSISlot{ID: 2, Device: &pb.PbDevice{Type: deviceType}}, 1); got != want {
			t.Fatalf("slot label for %s = %q, want %q", deviceType, got, want)
		}
	}
}
