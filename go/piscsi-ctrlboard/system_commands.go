// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import "fmt"

// SystemCommandKind identifies a privileged host operation requested from the
// System Commands submenu.
type SystemCommandKind string

const (
	SystemCommandReboot   SystemCommandKind = "reboot"
	SystemCommandShutdown SystemCommandKind = "system"
)

// SystemCommandSelection retains the selected slot solely for consistent
// Return navigation; the command itself applies to the host, not the slot.
type SystemCommandSelection struct {
	Kind SystemCommandKind
	Slot SCSISlot
}

func NewSystemCommandsMenu(slot SCSISlot, pageSize int) (*Menu, error) {
	items := []MenuItem{
		{ID: "return", Label: "Return", Data: SlotAction{Kind: SlotActionReturn, Slot: slot}},
		{ID: "reboot", Label: "Reboot", Data: SystemCommandSelection{Kind: SystemCommandReboot, Slot: slot}},
		{ID: "shutdown", Label: "Shutdown", Data: SystemCommandSelection{Kind: SystemCommandShutdown, Slot: slot}},
	}
	return NewMenu("System Commands", items, pageSize)
}

func (kind SystemCommandKind) displayMessage() (string, error) {
	switch kind {
	case SystemCommandReboot:
		return "Rebooting", nil
	case SystemCommandShutdown:
		return "Shutting down", nil
	default:
		return "", fmt.Errorf("unsupported system command %q", kind)
	}
}
