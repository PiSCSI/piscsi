// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"fmt"
	"sort"

	pb "github.com/piscsi/piscsi/go/proto"
)

// NewDeviceInfoMenu renders the daemon's existing device data as a read-only
// menu. The data comes from the root SCSI refresh, so opening Info does not
// perform a network request on the select path.
func NewDeviceInfoMenu(slot SCSISlot, pageSize int) (*Menu, error) {
	items := []MenuItem{{ID: "return", Label: "Return", Data: SlotAction{Kind: SlotActionReturn, Slot: slot}}}
	device := slot.Device
	if device == nil {
		items = append(items, MenuItem{ID: "empty", Label: "(No device attached)"})
		return NewMenu(fmt.Sprintf("SCSI ID %d Info", slot.ID), items, pageSize)
	}
	items = append(items,
		infoItem("id", "ID   : %d", device.GetId()),
		infoItem("lun", "LUN  : %d", device.GetUnit()),
		infoItem("file", "File : %s", deviceFilename(device)),
		infoItem("type", "Type : %s", device.GetType()),
		infoItem("access", "R/RW : %s", deviceAccess(device)),
	)
	for _, parameter := range deviceParameters(device) {
		items = append(items, MenuItem{ID: "param:" + parameter, Label: "Prms : " + parameter})
	}
	items = append(items,
		infoItem("vendor", "Vndr : %s", emptyAsDash(device.GetVendor())),
		infoItem("product", "Prdct: %s", emptyAsDash(device.GetProduct())),
		infoItem("revision", "Rvisn: %s", emptyAsDash(device.GetRevision())),
		infoItem("block-size", "Blksz: %d", device.GetBlockSize()),
		infoItem("image-size", "Imgsz: %d", device.GetFile().GetSize()),
	)
	if device.GetProperties().GetRemovable() {
		media := "present"
		if device.GetStatus().GetRemoved() {
			media = "removed"
		}
		items = append(items, MenuItem{ID: "media", Label: "Media: " + media})
	}
	return NewMenu(fmt.Sprintf("SCSI ID %d Info", slot.ID), items, pageSize)
}

func infoItem(id, format string, values ...any) MenuItem {
	return MenuItem{ID: id, Label: fmt.Sprintf(format, values...)}
}

func deviceFilename(device *pb.PbDevice) string {
	if name := device.GetFile().GetName(); name != "" {
		return name
	}
	if name := device.GetParams()["file"]; name != "" {
		return name
	}
	return "-"
}

func deviceAccess(device *pb.PbDevice) string {
	if device.GetProperties().GetReadOnly() || device.GetStatus().GetProtected() || device.GetFile().GetReadOnly() {
		return "Read-only"
	}
	return "Read/Write"
}

func deviceParameters(device *pb.PbDevice) []string {
	parameters := make([]string, 0, len(device.GetParams()))
	for key, value := range device.GetParams() {
		parameters = append(parameters, key+"="+value)
	}
	sort.Strings(parameters)
	return parameters
}

func emptyAsDash(value string) string {
	if value == "" {
		return "-"
	}
	return value
}
