package configuration

import (
	"os"
	"path/filepath"
	"reflect"
	"testing"

	"github.com/piscsi/piscsi/go/piscsi"
	pb "github.com/piscsi/piscsi/go/proto"
)

type fakeClient struct{ commands []*pb.PbCommand }

func (c *fakeClient) SendCommand(command *pb.PbCommand) (*pb.PbResult, error) {
	c.commands = append(c.commands, command)
	return &pb.PbResult{Status: true}, nil
}

func TestLoaderValidatesBeforeMutatingDaemon(t *testing.T) {
	directory := t.TempDir()
	if err := os.WriteFile(filepath.Join(directory, "bad.json"), []byte(`{"devices":[{"id":8}],"reserved_ids":[]}`), 0o644); err != nil {
		t.Fatal(err)
	}
	client := &fakeClient{}
	loader := Loader{ConfigDir: directory, ImageDir: directory, Client: client, Commands: piscsi.NewCommandBuilder()}
	if err := loader.Load("bad"); err == nil {
		t.Fatal("Load accepted an invalid configuration")
	}
	if len(client.commands) != 0 {
		t.Fatalf("sent %d commands before validation completed", len(client.commands))
	}
}

func TestLoaderAppliesValidatedConfigurationInOrder(t *testing.T) {
	configDir, imageDir := t.TempDir(), t.TempDir()
	data := `{"devices":[{"id":2,"unit":0,"device_type":"SCHD","image":"disk.hds","params":{}}],"reserved_ids":[{"id":6}]}`
	if err := os.WriteFile(filepath.Join(configDir, "profile.json"), []byte(data), 0o644); err != nil {
		t.Fatal(err)
	}
	client := &fakeClient{}
	loader := Loader{ConfigDir: configDir, ImageDir: imageDir, Client: client, Commands: piscsi.NewCommandBuilder()}
	if err := loader.Load("profile"); err != nil {
		t.Fatal(err)
	}
	operations := make([]pb.PbOperation, len(client.commands))
	for index, command := range client.commands {
		operations[index] = command.GetOperation()
	}
	want := []pb.PbOperation{pb.PbOperation_DETACH_ALL, pb.PbOperation_RESERVE_IDS, pb.PbOperation_ATTACH}
	if !reflect.DeepEqual(operations, want) {
		t.Fatalf("operations = %v, want %v", operations, want)
	}
	if got, want := client.commands[2].GetDevices()[0].GetParams()["file"], filepath.Join(imageDir, "disk.hds"); got != want {
		t.Fatalf("image path = %q, want %q", got, want)
	}
}

func TestParseValidatesSASILUNRange(t *testing.T) {
	valid := []byte(`{"devices":[{"id":2,"unit":1,"device_type":"SAHD","image":"disk.img","params":{}}],"reserved_ids":[]}`)
	_, devices, _, err := Parse(valid)
	if err != nil {
		t.Fatalf("Parse(valid SASI configuration): %v", err)
	}
	if len(devices) != 1 || devices[0].GetType() != pb.PbDeviceType_SAHD {
		t.Fatalf("parsed devices = %#v, want one SAHD", devices)
	}

	invalid := []byte(`{"devices":[{"id":2,"unit":2,"device_type":"SAHD","params":{}}],"reserved_ids":[]}`)
	if _, _, _, err := Parse(invalid); err == nil {
		t.Fatal("Parse accepted SASI LUN 2")
	}
}
