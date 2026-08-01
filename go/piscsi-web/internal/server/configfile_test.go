package server

import (
	"io"
	"log/slog"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"

	"github.com/piscsi/piscsi/go/piscsi-web/internal/config"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/server/testutil"
	pb "github.com/piscsi/piscsi/go/proto"
)

func TestConfigurationRoundTrip(t *testing.T) {
	info := &pb.PbServerInfo{
		VersionInfo: &pb.PbVersionInfo{
			MajorVersion: 25,
			MinorVersion: 1,
			PatchVersion: -1,
		},
		DevicesInfo: &pb.PbDevicesInfo{
			Devices: []*pb.PbDevice{{
				Id:        2,
				Unit:      1,
				Type:      pb.PbDeviceType_SCHD,
				File:      &pb.PbImageFile{Name: "subdir/system.hds"},
				Params:    map[string]string{"caching_mode": "piscsi"},
				Vendor:    "ACME",
				Product:   "Disk",
				Revision:  "1.0",
				BlockSize: 512,
				Status:    &pb.PbDeviceStatus{Protected: true},
			}},
		},
		ReservedIdsInfo: &pb.PbReservedIdsInfo{Ids: []int32{6}},
	}

	data, err := marshalConfiguration(info)
	if err != nil {
		t.Fatalf("marshalConfiguration() error = %v", err)
	}
	config, devices, reserved, err := parseConfiguration(data)
	if err != nil {
		t.Fatalf("parseConfiguration() error = %v", err)
	}

	if config.Version != "25.1.-1" {
		t.Fatalf("version = %q, want 25.1.-1", config.Version)
	}
	if !reflect.DeepEqual(reserved, []int32{6}) {
		t.Fatalf("reserved IDs = %v, want [6]", reserved)
	}
	if strings.Contains(string(data), `"memo"`) {
		t.Fatalf("configuration unexpectedly contains reservation memo: %s", data)
	}
	if len(devices) != 1 {
		t.Fatalf("devices count = %d, want 1", len(devices))
	}
	device := devices[0]
	if device.GetId() != 2 || device.GetUnit() != 1 || device.GetType() != pb.PbDeviceType_SCHD {
		t.Fatalf("unexpected device definition: %v", device)
	}
	if device.GetParams()["file"] != "subdir/system.hds" {
		t.Fatalf("file parameter = %q", device.GetParams()["file"])
	}
	if !device.GetProtected() || device.GetBlockSize() != 512 {
		t.Fatalf("protected/block size were not preserved: %v", device)
	}
}

func TestParseConfigurationRejectsInvalidData(t *testing.T) {
	tests := map[string]string{
		"invalid JSON":     `{`,
		"missing arrays":   `{}`,
		"null document":    `null`,
		"unknown type":     `{"devices":[{"id":1,"unit":0,"device_type":"NOPE","params":{}}]}`,
		"invalid ID":       `{"devices":[{"id":8,"unit":0,"device_type":"SCHD","params":{}}]}`,
		"duplicate device": `{"devices":[{"id":1,"unit":0,"device_type":"SCHD","params":{}},{"id":1,"unit":0,"device_type":"SCCD","params":{}}]}`,
		"reserved conflict": `{"devices":[{"id":1,"unit":0,"device_type":"SCHD","params":{}}],
			"reserved_ids":[{"id":1,"memo":""}]}`,
		"legacy list format": `[{"id":1,"un":0,"device_type":"SCHD","params":{}}]`,
		"trailing JSON":      `{"devices":[]} {}`,
	}

	for name, input := range tests {
		t.Run(name, func(t *testing.T) {
			if _, _, _, err := parseConfiguration([]byte(input)); err == nil {
				t.Fatal("parseConfiguration() accepted invalid data")
			}
		})
	}
}

func TestLoadConfigurationValidatesBeforeDetach(t *testing.T) {
	configDir := t.TempDir()
	if err := os.WriteFile(filepath.Join(configDir, defaultConfigFilename), []byte(`{"devices":[{"id":9}]}`), 0644); err != nil {
		t.Fatal(err)
	}

	commandCount := 0
	server := &Server{
		config: &config.Config{ConfigDir: configDir},
		piscsiClient: &testutil.MockPiSCSIClient{
			SendCommandFunc: func(_ *pb.PbCommand) (*pb.PbResult, error) {
				commandCount++
				return &pb.PbResult{Status: true}, nil
			},
		},
		logger: slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	if loaded, err := server.loadDefaultConfiguration(); err == nil || loaded {
		t.Fatal("loadDefaultConfiguration() accepted invalid data")
	}
	if commandCount != 0 {
		t.Fatalf("sent %d commands before validation completed", commandCount)
	}
}

func TestLoadConfigurationCommandSequence(t *testing.T) {
	configDir := t.TempDir()
	data := `{
		"version":"25.1.0",
		"devices":[{"id":2,"unit":0,"device_type":"SCHD","image":"disk.hds","params":{},"vendor":null,"product":null,"revision":null,"block_size":512}],
		"reserved_ids":[{"id":6,"memo":""}]
	}`
	if err := os.WriteFile(filepath.Join(configDir, "default.json"), []byte(data), 0644); err != nil {
		t.Fatal(err)
	}

	var operations []pb.PbOperation
	server := &Server{
		config: &config.Config{ConfigDir: configDir, BaseDir: "/var/lib/piscsi/images"},
		piscsiClient: &testutil.MockPiSCSIClient{
			SendCommandFunc: func(command *pb.PbCommand) (*pb.PbResult, error) {
				operations = append(operations, command.GetOperation())
				if command.GetOperation() == pb.PbOperation_ATTACH && command.GetDevices()[0].GetParams()["file"] != "/var/lib/piscsi/images/disk.hds" {
					t.Fatalf("attach command did not restore image: %v", command)
				}
				return &pb.PbResult{Status: true}, nil
			},
		},
		logger: slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	loaded, err := server.loadDefaultConfiguration()
	if err != nil {
		t.Fatalf("loadDefaultConfiguration() error = %v", err)
	}
	if !loaded {
		t.Fatal("loadDefaultConfiguration() did not report loading default.json")
	}
	want := []pb.PbOperation{
		pb.PbOperation_DETACH_ALL,
		pb.PbOperation_RESERVE_IDS,
		pb.PbOperation_ATTACH,
	}
	if !reflect.DeepEqual(operations, want) {
		t.Fatalf("operations = %v, want %v", operations, want)
	}
}

func TestLoadDefaultConfigurationMissingDoesNothing(t *testing.T) {
	configDir := t.TempDir()
	commandCount := 0
	server := &Server{
		config: &config.Config{ConfigDir: configDir},
		piscsiClient: &testutil.MockPiSCSIClient{
			SendCommandFunc: func(_ *pb.PbCommand) (*pb.PbResult, error) {
				commandCount++
				return &pb.PbResult{Status: true}, nil
			},
		},
		logger: slog.New(slog.NewTextHandler(io.Discard, nil)),
	}

	loaded, err := server.loadDefaultConfiguration()
	if err != nil {
		t.Fatalf("loadDefaultConfiguration() error = %v", err)
	}
	if loaded {
		t.Fatal("loadDefaultConfiguration() reported loading a missing file")
	}
	if commandCount != 0 {
		t.Fatalf("missing default configuration sent %d daemon commands", commandCount)
	}
}

func TestLegacyConfigurationErrorIncludesMigrationPath(t *testing.T) {
	_, _, _, err := parseConfiguration([]byte(`[{"id":1,"un":0,"device_type":"SCHD","params":{}}]`))
	if err == nil {
		t.Fatal("parseConfiguration() accepted legacy list format")
	}
	if !strings.Contains(err.Error(), "Python web client") || !strings.Contains(err.Error(), "migrate") {
		t.Fatalf("legacy format error lacks migration guidance: %v", err)
	}
}

func TestNormalizeConfigFilename(t *testing.T) {
	got, err := normalizeConfigFilename("default")
	if err != nil {
		t.Fatalf("normalizeConfigFilename() error = %v", err)
	}
	if got != "default.json" {
		t.Fatalf("normalizeConfigFilename() = %q", got)
	}
	for _, name := range []string{"../default", "default.properties", strings.Repeat("a", 256)} {
		if _, err := normalizeConfigFilename(name); err == nil {
			t.Fatalf("normalizeConfigFilename(%q) accepted invalid name", name)
		}
	}
}
