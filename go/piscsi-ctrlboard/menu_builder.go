// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"context"
	"fmt"
	"path/filepath"
	"sort"
	"time"

	"github.com/piscsi/piscsi/go/piscsi"
	pb "github.com/piscsi/piscsi/go/proto"
)

// CommandSender is satisfied by the shared PiSCSI client.
type CommandSender interface {
	SendCommand(*pb.PbCommand) (*pb.PbResult, error)
}

// SCSISlot describes the state behind one of the root menu's IDs.
type SCSISlot struct {
	ID       int32
	Reserved bool
	Device   *pb.PbDevice
}

// SCSIMenuBuilder reads current daemon state and turns it into the root menu.
type SCSIMenuBuilder struct {
	client   CommandSender
	commands *piscsi.CommandBuilder
}

func NewSCSIMenuBuilder(client CommandSender, token string) *SCSIMenuBuilder {
	commands := piscsi.NewCommandBuilder()
	commands.SetToken(token)
	return &SCSIMenuBuilder{client: client, commands: commands}
}

func (b *SCSIMenuBuilder) Build(ctx context.Context) ([]MenuItem, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if b == nil || b.client == nil || b.commands == nil {
		return nil, fmt.Errorf("SCSI menu builder is not initialized")
	}
	result, err := b.client.SendCommand(b.commands.ServerInfo())
	if err != nil {
		return nil, fmt.Errorf("get PiSCSI server info: %w", err)
	}
	if !result.GetStatus() {
		return nil, fmt.Errorf("get PiSCSI server info: %s", result.GetMsg())
	}
	info := result.GetServerInfo()
	if info == nil {
		return nil, fmt.Errorf("get PiSCSI server info: response is missing server information")
	}
	reserved := make(map[int32]bool, len(info.GetReservedIdsInfo().GetIds()))
	for _, id := range info.GetReservedIdsInfo().GetIds() {
		reserved[id] = true
	}
	devices := make(map[int32][]*pb.PbDevice)
	for _, device := range info.GetDevicesInfo().GetDevices() {
		devices[device.GetId()] = append(devices[device.GetId()], device)
	}
	items := make([]MenuItem, 0, 8)
	for id := int32(0); id <= 7; id++ {
		slot := SCSISlot{ID: id, Reserved: reserved[id]}
		entries := devices[id]
		sort.Slice(entries, func(left, right int) bool { return entries[left].GetUnit() < entries[right].GetUnit() })
		if len(entries) > 0 {
			slot.Device = entries[0]
		}
		items = append(items, MenuItem{ID: fmt.Sprintf("scsi-%d", id), Label: slotLabel(slot, len(entries)), Data: slot})
	}
	return items, nil
}

func slotLabel(slot SCSISlot, deviceCount int) string {
	if slot.Reserved {
		return fmt.Sprintf("%d: [Reserved]", slot.ID)
	}
	if slot.Device == nil {
		return fmt.Sprintf("%d: (empty)", slot.ID)
	}
	if slot.Device.GetType() == pb.PbDeviceType_SCBR {
		return fmt.Sprintf("%d: Host Bridge", slot.ID)
	}
	if slot.Device.GetType() == pb.PbDeviceType_SCDP {
		return fmt.Sprintf("%d: Daynaport", slot.ID)
	}
	if slot.Device.GetType() == pb.PbDeviceType_SCLP {
		return fmt.Sprintf("%d: SCSI Printer", slot.ID)
	}
	if slot.Device.GetType() == pb.PbDeviceType_SCHS {
		return fmt.Sprintf("%d: Host Services", slot.ID)
	}
	name := slot.Device.GetFile().GetName()
	if name == "" {
		name = slot.Device.GetParams()["file"]
	}
	if name == "" {
		name = "(empty)"
	} else {
		name = filepath.Base(name)
	}
	if deviceCount > 1 {
		name = fmt.Sprintf("%s (+%d LUN)", name, deviceCount-1)
	}
	return fmt.Sprintf("%d: %s [%s]", slot.ID, name, slot.Device.GetType())
}

// SCSIRefresher polls outside the input event path and updates only the menu
// model. Rendering is coalesced by MenuController.
type SCSIRefresher struct {
	builder  *SCSIMenuBuilder
	menu     *MenuController
	interval time.Duration
	onError  func(error)
	requests chan struct{}
}

func NewSCSIRefresher(builder *SCSIMenuBuilder, menu *MenuController, interval time.Duration, onError func(error)) (*SCSIRefresher, error) {
	if builder == nil || menu == nil {
		return nil, fmt.Errorf("SCSI builder and menu controller are required")
	}
	if interval <= 0 {
		return nil, fmt.Errorf("SCSI refresh interval must be positive")
	}
	return &SCSIRefresher{builder: builder, menu: menu, interval: interval, onError: onError, requests: make(chan struct{}, 1)}, nil
}

// RequestRefresh schedules an immediate root-menu refresh. Requests coalesce,
// so completing several operations cannot create a daemon-request backlog.
func (r *SCSIRefresher) RequestRefresh() {
	if r == nil {
		return
	}
	select {
	case r.requests <- struct{}{}:
	default:
	}
}

func (r *SCSIRefresher) Run(ctx context.Context) {
	r.refresh(ctx)
	ticker := time.NewTicker(r.interval)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-r.requests:
			r.refresh(ctx)
		case <-ticker.C:
			r.refresh(ctx)
		}
	}
}

func (r *SCSIRefresher) refresh(ctx context.Context) {
	items, err := r.builder.Build(ctx)
	if err != nil {
		if r.onError != nil && ctx.Err() == nil {
			r.onError(err)
		}
		return
	}
	r.menu.ReplaceRootItems(items)
}
