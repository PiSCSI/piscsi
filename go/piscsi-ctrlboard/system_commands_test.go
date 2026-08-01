// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"context"
	"testing"

	pb "github.com/piscsi/piscsi/go/proto"
)

func TestNewSystemCommandsMenu(t *testing.T) {
	menu, err := NewSystemCommandsMenu(SCSISlot{ID: 2}, 4)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := len(menu.Items), 3; got != want {
		t.Fatalf("items = %d, want %d", got, want)
	}
	if selected, ok := menu.Items[2].Data.(SystemCommandSelection); !ok || selected.Kind != SystemCommandShutdown {
		t.Fatalf("shutdown item = %#v", menu.Items[2].Data)
	}
}

func TestRunSystemCommandSendsDaemonShutdown(t *testing.T) {
	client := &workflowClient{results: []*pb.PbResult{{Status: true}}}
	message, err := NewSCSIWorkflow(client, "").RunSystemCommand(context.Background(), SystemCommandSelection{Kind: SystemCommandReboot})
	if err != nil {
		t.Fatal(err)
	}
	if got, want := message, "Rebooting"; got != want {
		t.Fatalf("message = %q, want %q", got, want)
	}
	command := client.commands[0]
	if got, want := command.GetOperation(), pb.PbOperation_SHUT_DOWN; got != want {
		t.Fatalf("operation = %s, want %s", got, want)
	}
	if got, want := command.GetParams()["mode"], "reboot"; got != want {
		t.Fatalf("mode = %q, want %q", got, want)
	}
}
