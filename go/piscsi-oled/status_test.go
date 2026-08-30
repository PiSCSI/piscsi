package oled

import (
	"context"
	"errors"
	"reflect"
	"testing"

	pb "github.com/piscsi/piscsi/go/proto"
)

type fakeSender struct {
	results []*pb.PbResult
	err     error
	calls   int
}

func (f *fakeSender) SendCommand(*pb.PbCommand) (*pb.PbResult, error) {
	if f.err != nil {
		return nil, f.err
	}
	if f.calls >= len(f.results) {
		return nil, errors.New("unexpected command")
	}
	result := f.results[f.calls]
	f.calls++
	return result, nil
}

func TestFormat(t *testing.T) {
	monitor := NewMonitor(&fakeSender{}, "")
	monitor.ip, monitor.hostname = "192.0.2.5", "piscsi"
	monitor.removable[pb.PbDeviceType_SCCD] = true

	t.Run("no devices", func(t *testing.T) {
		want := []string{"No device attached!", "IP 192.0.2.5 - piscsi"}
		if got := monitor.Format(nil); !reflect.DeepEqual(got, want) {
			t.Errorf("Format() = %#v, want %#v", got, want)
		}
	})
	t.Run("media and LUNs", func(t *testing.T) {
		devices := []*pb.PbDevice{
			{Id: 2, Unit: 0, Type: pb.PbDeviceType_SCCD},
			{Id: 2, Unit: 1, Type: pb.PbDeviceType_SCHD, File: &pb.PbImageFile{Name: "/var/lib/piscsi/images/disk.hds"}, Vendor: "ACME", Product: "Drive"},
		}
		want := []string{"2 0 CD [No Media]", "2 1 HD disk.hds ACME Drive", "IP 192.0.2.5 - piscsi"}
		if got := monitor.Format(devices); !reflect.DeepEqual(got, want) {
			t.Errorf("Format() = %#v, want %#v", got, want)
		}
	})
	t.Run("file parameter path", func(t *testing.T) {
		devices := []*pb.PbDevice{{Id: 3, Type: pb.PbDeviceType_SCHD, Params: map[string]string{"file": "/var/lib/piscsi/images/DEC_RZ22.hd1"}}}
		want := []string{"3 HD DEC_RZ22.hd1", "IP 192.0.2.5 - piscsi"}
		if got := monitor.Format(devices); !reflect.DeepEqual(got, want) {
			t.Errorf("Format() = %#v, want %#v", got, want)
		}
	})
	t.Run("network absent", func(t *testing.T) {
		monitor.ip = ""
		want := []string{"No device attached!", "No network connection"}
		if got := monitor.Format(nil); !reflect.DeepEqual(got, want) {
			t.Errorf("Format() = %#v, want %#v", got, want)
		}
	})
}

func TestFormatStatusSeparatesFixedPrefixFromParameter(t *testing.T) {
	monitor := NewMonitor(&fakeSender{}, "")
	monitor.ip, monitor.hostname = "192.0.2.5", "piscsi"
	devices := []*pb.PbDevice{{
		Id:   3,
		Type: pb.PbDeviceType_SCHD,
		File: &pb.PbImageFile{Name: "/var/lib/piscsi/images/a-long-disk-name.hds"},
	}}
	want := []StatusLine{
		{Fixed: "3 HD", Parameter: "a-long-disk-name.hds"},
		{Parameter: "IP 192.0.2.5 - piscsi"},
	}
	if got := monitor.FormatStatus(devices); !reflect.DeepEqual(got, want) {
		t.Errorf("FormatStatus() = %#v, want %#v", got, want)
	}
}

func TestFormatStatusUsesProductForNonFileDevices(t *testing.T) {
	monitor := NewMonitor(&fakeSender{}, "")
	monitor.ip, monitor.hostname = "192.0.2.5", "piscsi"
	devices := []*pb.PbDevice{
		{
			Id: 3, Type: pb.PbDeviceType_SCHS,
			Properties: &pb.PbDeviceProperties{SupportsFile: false},
			Vendor:     "PiSCSI",
			Product:    "Host Services",
		},
		{
			Id: 4, Type: pb.PbDeviceType_SCDP,
			Properties: &pb.PbDeviceProperties{SupportsFile: false},
			Vendor:     "Dayna",
			Product:    "SCSI/Link",
		},
	}
	want := []StatusLine{
		{Fixed: "3 HS", Parameter: "Host Services"},
		{Fixed: "4 DP", Parameter: "Dayna SCSI/Link"},
		{Parameter: "IP 192.0.2.5 - piscsi"},
	}
	if got := monitor.FormatStatus(devices); !reflect.DeepEqual(got, want) {
		t.Errorf("FormatStatus() = %#v, want %#v", got, want)
	}
}

func TestTypeSuffix(t *testing.T) {
	if got := typeSuffix(pb.PbDeviceType_SCCD); got != "CD" {
		t.Errorf("typeSuffix(SCCD) = %q, want CD", got)
	}
	if got := typeSuffix(pb.PbDeviceType_SAHD); got != "SA" {
		t.Errorf("typeSuffix(SAHD) = %q, want SA", got)
	}
}

func TestPollAuthenticationAndDaemonFailures(t *testing.T) {
	t.Run("permission denied", func(t *testing.T) {
		monitor := NewMonitor(&fakeSender{results: []*pb.PbResult{{Status: false}}}, "")
		monitor.network = func() (string, string) { return "", "piscsi" }
		got, err := monitor.Poll(context.Background())
		if err != nil {
			t.Fatal(err)
		}
		want := []string{"Permission denied!", "No network connection"}
		if !reflect.DeepEqual(got, want) {
			t.Errorf("Poll() = %#v, want %#v", got, want)
		}
	})
	t.Run("daemon error", func(t *testing.T) {
		monitor := NewMonitor(&fakeSender{err: errors.New("offline")}, "")
		if _, err := monitor.Poll(context.Background()); err == nil {
			t.Fatal("Poll() error = nil, want daemon error")
		}
	})
}

func TestLoadDeviceTypes(t *testing.T) {
	result := &pb.PbResult{Status: true, Result: &pb.PbResult_DeviceTypesInfo{DeviceTypesInfo: &pb.PbDeviceTypesInfo{Properties: []*pb.PbDeviceTypeProperties{{
		Type: pb.PbDeviceType_SCCD, Properties: &pb.PbDeviceProperties{Removable: true},
	}}}}}
	monitor := NewMonitor(&fakeSender{results: []*pb.PbResult{result}}, "")
	if err := monitor.LoadDeviceTypes(context.Background()); err != nil {
		t.Fatal(err)
	}
	if !monitor.removable[pb.PbDeviceType_SCCD] {
		t.Fatal("removable metadata was not cached")
	}
}
