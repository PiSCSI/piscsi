// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"fmt"
	"sort"

	pb "github.com/piscsi/piscsi/go/proto"
)

const defaultPrinterCommand = "lp -oraw %f"

// DeviceTypeSelection retains the slot and daemon-advertised capabilities as
// the attachment flow moves from a device type to its options.
type DeviceTypeSelection struct {
	Slot       SCSISlot
	Type       pb.PbDeviceType
	Properties *pb.PbDeviceProperties
}

// DeviceAttachSelection is a complete file-less attach request. It is used
// for devices such as Host Services and for the default printer workflow.
type DeviceAttachSelection struct {
	Slot   SCSISlot
	Type   pb.PbDeviceType
	Params map[string]string
}

// NetworkTopologySelection identifies the daemon-advertised network mode and
// host interface selected by the user.
type NetworkTopologySelection struct {
	Slot      SCSISlot
	Type      pb.PbDeviceType
	Mode      string
	Interface string
}

// NewDeviceTypeMenu presents only the device types currently supported by the
// connected daemon. SAHD remains last, matching the web interface.
func NewDeviceTypeMenu(slot SCSISlot, properties []*pb.PbDeviceTypeProperties, pageSize int) (*Menu, error) {
	types := append([]*pb.PbDeviceTypeProperties(nil), properties...)
	sort.SliceStable(types, func(i, j int) bool {
		left, right := types[i].GetType(), types[j].GetType()
		if left == pb.PbDeviceType_SAHD {
			return false
		}
		if right == pb.PbDeviceType_SAHD {
			return true
		}
		return left < right
	})
	items := []MenuItem{{ID: "return", Label: "Return", Data: SlotAction{Kind: SlotActionReturn, Slot: slot}}}
	for _, typeProperties := range types {
		if typeProperties == nil || typeProperties.GetType() == pb.PbDeviceType_UNDEFINED {
			continue
		}
		deviceType := typeProperties.GetType()
		items = append(items, MenuItem{
			ID:    "device:" + deviceType.String(),
			Label: fmt.Sprintf("%s [%s]", deviceTypeName(deviceType), deviceType),
			Data:  DeviceTypeSelection{Slot: slot, Type: deviceType, Properties: typeProperties.GetProperties()},
		})
	}
	if len(items) == 1 {
		items = append(items, MenuItem{ID: "empty", Label: "(No device types available)"})
	}
	return NewMenu("Select Device", items, pageSize)
}

// NewDeviceOptionMenu holds the final confirmation for non-file devices. The
// daemon defaults are applied by the workflow when the item is selected.
func NewDeviceOptionMenu(selection DeviceTypeSelection, pageSize int) (*Menu, error) {
	items := []MenuItem{
		{ID: "return", Label: "Return", Data: SlotAction{Kind: SlotActionReturn, Slot: selection.Slot}},
		{ID: "attach", Label: deviceAttachLabel(selection.Type), Data: DeviceAttachSelection{
			Slot: selection.Slot, Type: selection.Type, Params: copyParams(selection.Properties.GetDefaultParams()),
		}},
	}
	return NewMenu(deviceTypeName(selection.Type), items, pageSize)
}

// AddAttachWithoutMediaOption adds the naked removable-device attachment at
// the top of a new-device image picker. It must not be used for INSERT flows,
// where a removable device is already attached.
func AddAttachWithoutMediaOption(menu *Menu, selection DeviceTypeSelection) {
	if menu == nil || !selection.Properties.GetRemovable() {
		return
	}
	item := MenuItem{
		ID:    "attach-empty:" + selection.Type.String(),
		Label: "Attach with no media",
		Data: DeviceAttachSelection{
			Slot: selection.Slot, Type: selection.Type, Params: copyParams(selection.Properties.GetDefaultParams()),
		},
	}
	if len(menu.Items) == 0 {
		menu.Items = []MenuItem{item}
		return
	}
	menu.Items = append(menu.Items, MenuItem{})
	copy(menu.Items[2:], menu.Items[1:len(menu.Items)-1])
	menu.Items[1] = item
}

// NewNetworkTopologyMenu shows the network profiles actively advertised by the
// daemon. Down interfaces and unsupported modes are deliberately omitted.
func NewNetworkTopologyMenu(slot SCSISlot, deviceType pb.PbDeviceType, interfaces []*pb.PbNetworkInterface, pageSize int) (*Menu, error) {
	profiles := make([]NetworkTopologySelection, 0)
	for _, networkInterface := range interfaces {
		if networkInterface == nil || !networkInterface.GetUp() || networkInterface.GetName() == "" {
			continue
		}
		for _, mode := range networkInterface.GetSupportedMode() {
			if mode == "bridge" || mode == "proxyarp" {
				profiles = append(profiles, NetworkTopologySelection{Slot: slot, Type: deviceType, Mode: mode, Interface: networkInterface.GetName()})
			}
		}
	}
	sort.Slice(profiles, func(i, j int) bool {
		if profiles[i].Mode == profiles[j].Mode {
			return profiles[i].Interface < profiles[j].Interface
		}
		return profiles[i].Mode == "bridge"
	})
	items := []MenuItem{{ID: "return", Label: "Return", Data: SlotAction{Kind: SlotActionReturn, Slot: slot}}}
	for _, profile := range profiles {
		label := "Wi-Fi proxy ARP"
		if profile.Mode == "bridge" {
			label = "Wired bridge"
		}
		items = append(items, MenuItem{
			ID:    fmt.Sprintf("topology:%s:%s", profile.Mode, profile.Interface),
			Label: fmt.Sprintf("%s (%s)", label, profile.Interface),
			Data:  profile,
		})
	}
	if len(items) == 1 {
		items = append(items, MenuItem{ID: "empty", Label: "(No network topologies available)"})
	}
	return NewMenu("Select Topology", items, pageSize)
}

func deviceTypeName(deviceType pb.PbDeviceType) string {
	switch deviceType {
	case pb.PbDeviceType_SAHD:
		return "SASI Hard Disk"
	case pb.PbDeviceType_SCHD:
		return "SCSI Hard Disk"
	case pb.PbDeviceType_SCRM:
		return "SCSI Removable Media"
	case pb.PbDeviceType_SCMO:
		return "SCSI Magneto-Optical"
	case pb.PbDeviceType_SCCD:
		return "SCSI CD-ROM"
	case pb.PbDeviceType_SCBR:
		return "Host Bridge"
	case pb.PbDeviceType_SCDP:
		return "Ethernet Adapter"
	case pb.PbDeviceType_SCHS:
		return "Host Services"
	case pb.PbDeviceType_SCLP:
		return "Printer"
	case pb.PbDeviceType_SCTP:
		return "SCSI Tape"
	default:
		return deviceType.String()
	}
}

func deviceAttachLabel(deviceType pb.PbDeviceType) string {
	if deviceType == pb.PbDeviceType_SCLP {
		return "Attach Printer (" + defaultPrinterCommand + ")"
	}
	return "Attach " + deviceTypeName(deviceType)
}

func copyParams(params map[string]string) map[string]string {
	if len(params) == 0 {
		return nil
	}
	copied := make(map[string]string, len(params))
	for key, value := range params {
		copied[key] = value
	}
	return copied
}
