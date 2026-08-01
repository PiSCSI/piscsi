// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import "fmt"

// SlotActionKind identifies a workflow selected from a SCSI slot submenu.
type SlotActionKind string

const (
	SlotActionReturn       SlotActionKind = "return"
	SlotActionAttachInsert SlotActionKind = "attach_insert"
	SlotActionDetachEject  SlotActionKind = "detach_eject"
	SlotActionReserve      SlotActionKind = "reserve"
	SlotActionRelease      SlotActionKind = "release"
	SlotActionInfo         SlotActionKind = "info"
	SlotActionLoadProfile  SlotActionKind = "load_profile"
	SlotActionSystemInfo   SlotActionKind = "system_info"
	SlotActionSystemCmds   SlotActionKind = "system_commands"
)

// SlotAction retains the selected SCSI slot as navigation moves into a child
// menu. It will be consumed by the asynchronous action dispatcher.
type SlotAction struct {
	Kind SlotActionKind
	Slot SCSISlot
}

func NewSlotActionMenu(slot SCSISlot, pageSize int) (*Menu, error) {
	entries := []struct {
		id, label string
		kind      SlotActionKind
	}{
		{"return", "Return", SlotActionReturn},
		{"attach", "Attach/Insert", SlotActionAttachInsert},
		{"detach", "Detach/Eject", SlotActionDetachEject},
		{"info", "Info", SlotActionInfo},
	}
	if slot.Reserved {
		entries = append(entries, struct {
			id, label string
			kind      SlotActionKind
		}{"release", "Release", SlotActionRelease})
	} else {
		entries = append(entries, struct {
			id, label string
			kind      SlotActionKind
		}{"reserve", "Reserve", SlotActionReserve})
	}
	entries = append(entries,
		struct {
			id, label string
			kind      SlotActionKind
		}{"profile", "Load Profile", SlotActionLoadProfile},
		struct {
			id, label string
			kind      SlotActionKind
		}{"system-info", "System Info", SlotActionSystemInfo},
		struct {
			id, label string
			kind      SlotActionKind
		}{"system-commands", "System Commands", SlotActionSystemCmds},
	)
	items := make([]MenuItem, 0, len(entries))
	for _, entry := range entries {
		items = append(items, MenuItem{ID: entry.id, Label: entry.label, Data: SlotAction{Kind: entry.kind, Slot: slot}})
	}
	return NewMenu(fmt.Sprintf("SCSI ID %d", slot.ID), items, pageSize)
}
