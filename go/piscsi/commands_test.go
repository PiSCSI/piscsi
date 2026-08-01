package piscsi

import (
	"testing"

	pb "github.com/piscsi/piscsi/go/proto"
)

func TestCommandsIncludeTokenAndLocale(t *testing.T) {
	builder := NewCommandBuilder()
	builder.SetToken("daemon-secret")
	builder.SetLocale("sv")

	commands := []*pb.PbCommand{
		builder.ServerInfo(),
		builder.AttachDevice(1, 0, pb.PbDeviceType_SCHD, "disk.hds", 0, nil),
		builder.ShutDown("reboot"),
		builder.CheckAuthentication(),
	}
	for _, command := range commands {
		if got := command.GetParams()["token"]; got != "daemon-secret" {
			t.Errorf("%s token = %q, want configured token", command.GetOperation(), got)
		}
		if got := command.GetParams()["locale"]; got != "sv" {
			t.Errorf("%s locale = %q, want sv", command.GetOperation(), got)
		}
	}
}

func TestCommandsIncludeEmptyTokenAndDefaultLocale(t *testing.T) {
	command := NewCommandBuilder().ServerInfo()
	if token, ok := command.GetParams()["token"]; !ok || token != "" {
		t.Errorf("token = %q, present = %v; want an explicit empty token", token, ok)
	}
	if got := command.GetParams()["locale"]; got != "en" {
		t.Errorf("locale = %q, want en", got)
	}
}

func TestCheckAuthentication(t *testing.T) {
	command := NewCommandBuilder().CheckAuthentication()
	if command.GetOperation() != pb.PbOperation_CHECK_AUTHENTICATION {
		t.Errorf("operation = %s, want CHECK_AUTHENTICATION", command.GetOperation())
	}
}

func TestShutDown(t *testing.T) {
	for _, mode := range []string{"system", "reboot"} {
		t.Run(mode, func(t *testing.T) {
			cmd := NewCommandBuilder().ShutDown(mode)
			if cmd.GetOperation() != pb.PbOperation_SHUT_DOWN {
				t.Errorf("operation = %s, want SHUT_DOWN", cmd.GetOperation())
			}
			if got := cmd.GetParams()["mode"]; got != mode {
				t.Errorf("mode = %q, want %q", got, mode)
			}
		})
	}
}
