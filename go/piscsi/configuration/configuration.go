// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

// Package configuration reads, validates, saves, and applies PiSCSI's
// object-style configuration files. It deliberately does not support the
// historical top-level-list format.
package configuration

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"

	"github.com/piscsi/piscsi/go/piscsi"
	pb "github.com/piscsi/piscsi/go/proto"
)

const FileSuffix = ".json"

// Configuration is the persisted object-format JSON document.
type Configuration struct {
	Version     string        `json:"version"`
	Devices     []Device      `json:"devices"`
	ReservedIDs []Reservation `json:"reserved_ids"`
}

// Device is a device entry in a Configuration.
type Device struct {
	ID         int32             `json:"id"`
	Unit       int32             `json:"unit"`
	DeviceType string            `json:"device_type"`
	Image      *string           `json:"image"`
	Params     map[string]string `json:"params"`
	Vendor     *string           `json:"vendor"`
	Product    *string           `json:"product"`
	Revision   *string           `json:"revision"`
	BlockSize  *int32            `json:"block_size"`
	Protected  bool              `json:"protected,omitempty"`
}

// Reservation intentionally omits the Python web client's memo field.
type Reservation struct {
	ID int32 `json:"id"`
}

// CommandSender is satisfied by the shared PiSCSI client.
type CommandSender interface {
	SendCommand(*pb.PbCommand) (*pb.PbResult, error)
}

// Loader applies one validated configuration file to a PiSCSI daemon.
type Loader struct {
	ConfigDir string
	ImageDir  string
	Client    CommandSender
	Commands  *piscsi.CommandBuilder
}

// NormalizeFilename validates a configuration basename and adds .json when
// the caller did not supply an extension.
func NormalizeFilename(name string) (string, error) {
	name = strings.TrimSpace(name)
	if name == "" {
		return "", fmt.Errorf("file name is required")
	}
	if filepath.Base(name) != name || !validFilename(name) {
		return "", fmt.Errorf("invalid configuration file name")
	}
	if filepath.Ext(name) == "" {
		name += FileSuffix
	}
	if !strings.EqualFold(filepath.Ext(name), FileSuffix) {
		return "", fmt.Errorf("configuration files must use the %s suffix", FileSuffix)
	}
	return name, nil
}

// Marshal serializes the current daemon state in the object-style format.
func Marshal(info *pb.PbServerInfo) ([]byte, error) {
	if info == nil {
		return nil, fmt.Errorf("PiSCSI server information is missing")
	}

	versionInfo := info.GetVersionInfo()
	config := Configuration{
		Version: fmt.Sprintf("%d.%d.%d", versionInfo.GetMajorVersion(), versionInfo.GetMinorVersion(), versionInfo.GetPatchVersion()),
		Devices: make([]Device, 0), ReservedIDs: make([]Reservation, 0),
	}
	for _, device := range info.GetDevicesInfo().GetDevices() {
		params := make(map[string]string, len(device.GetParams()))
		for key, value := range device.GetParams() {
			params[key] = value
		}
		saved := Device{ID: device.GetId(), Unit: device.GetUnit(), DeviceType: device.GetType().String(), Params: params, Protected: device.GetStatus().GetProtected()}
		if image := device.GetFile().GetName(); image != "" {
			saved.Image = stringPointer(image)
		}
		if device.GetVendor() != "PiSCSI" {
			if device.GetVendor() != "" {
				saved.Vendor = stringPointer(device.GetVendor())
			}
			if device.GetProduct() != "" {
				saved.Product = stringPointer(device.GetProduct())
			}
			if device.GetRevision() != "" {
				saved.Revision = stringPointer(device.GetRevision())
			}
		}
		if device.GetBlockSize() != 0 {
			blockSize := device.GetBlockSize()
			saved.BlockSize = &blockSize
		}
		config.Devices = append(config.Devices, saved)
	}
	for _, id := range info.GetReservedIdsInfo().GetIds() {
		config.ReservedIDs = append(config.ReservedIDs, Reservation{ID: id})
	}
	data, err := json.MarshalIndent(config, "", "    ")
	if err != nil {
		return nil, fmt.Errorf("encode configuration: %w", err)
	}
	return append(data, '\n'), nil
}

// Parse validates data before any daemon mutations and returns attach-ready
// device definitions plus reserved SCSI IDs.
func Parse(data []byte) (*Configuration, []*pb.PbDeviceDefinition, []int32, error) {
	trimmed := bytes.TrimSpace(data)
	if len(trimmed) > 0 && trimmed[0] == '[' {
		return nil, nil, nil, fmt.Errorf("legacy top-level-list configurations are not supported; load and re-save the file with the Python web client to migrate it")
	}
	decoder := json.NewDecoder(bytes.NewReader(data))
	var config Configuration
	if err := decoder.Decode(&config); err != nil {
		return nil, nil, nil, fmt.Errorf("parse configuration JSON: %w", err)
	}
	var trailing any
	if err := decoder.Decode(&trailing); err != io.EOF {
		return nil, nil, nil, fmt.Errorf("configuration contains trailing JSON data")
	}
	if config.Devices == nil || config.ReservedIDs == nil {
		return nil, nil, nil, fmt.Errorf("configuration must contain devices and reserved_ids arrays")
	}

	devices := make([]*pb.PbDeviceDefinition, 0, len(config.Devices))
	occupied := make(map[[2]int32]struct{}, len(config.Devices))
	usedIDs := make(map[int32]struct{}, len(config.Devices))
	for index, saved := range config.Devices {
		if saved.ID < 0 || saved.ID > 7 {
			return nil, nil, nil, fmt.Errorf("device %d has invalid SCSI ID %d", index, saved.ID)
		}
		typeValue, exists := pb.PbDeviceType_value[strings.ToUpper(saved.DeviceType)]
		if !exists || pb.PbDeviceType(typeValue) == pb.PbDeviceType_UNDEFINED {
			return nil, nil, nil, fmt.Errorf("device %d has invalid type %q", index, saved.DeviceType)
		}
		deviceType := pb.PbDeviceType(typeValue)
		if saved.Unit < 0 || saved.Unit > piscsi.MaxLUN(deviceType) {
			return nil, nil, nil, fmt.Errorf("device %d has invalid LUN %d for %s (must be 0-%d)", index, saved.Unit, deviceType, piscsi.MaxLUN(deviceType))
		}
		location := [2]int32{saved.ID, saved.Unit}
		if _, exists := occupied[location]; exists {
			return nil, nil, nil, fmt.Errorf("duplicate device at SCSI ID %d LUN %d", saved.ID, saved.Unit)
		}
		occupied[location], usedIDs[saved.ID] = struct{}{}, struct{}{}
		params := make(map[string]string, len(saved.Params)+1)
		for key, value := range saved.Params {
			params[key] = value
		}
		if saved.Image != nil && *saved.Image != "" {
			params["file"] = *saved.Image
		}
		definition := &pb.PbDeviceDefinition{Id: saved.ID, Unit: saved.Unit, Type: deviceType, Params: params, Protected: saved.Protected}
		if saved.Vendor != nil {
			definition.Vendor = *saved.Vendor
		}
		if saved.Product != nil {
			definition.Product = *saved.Product
		}
		if saved.Revision != nil {
			definition.Revision = *saved.Revision
		}
		if saved.BlockSize != nil {
			if *saved.BlockSize < 0 {
				return nil, nil, nil, fmt.Errorf("device %d has invalid block size %d", index, *saved.BlockSize)
			}
			definition.BlockSize = *saved.BlockSize
		}
		devices = append(devices, definition)
	}
	reservedIDs := make([]int32, 0, len(config.ReservedIDs))
	reserved := make(map[int32]struct{}, len(config.ReservedIDs))
	for _, reservation := range config.ReservedIDs {
		if reservation.ID < 0 || reservation.ID > 7 {
			return nil, nil, nil, fmt.Errorf("invalid reserved SCSI ID %d", reservation.ID)
		}
		if _, exists := reserved[reservation.ID]; exists {
			return nil, nil, nil, fmt.Errorf("duplicate reserved SCSI ID %d", reservation.ID)
		}
		if _, exists := usedIDs[reservation.ID]; exists {
			return nil, nil, nil, fmt.Errorf("SCSI ID %d is both attached and reserved", reservation.ID)
		}
		reserved[reservation.ID] = struct{}{}
		reservedIDs = append(reservedIDs, reservation.ID)
	}
	return &config, devices, reservedIDs, nil
}

// Load reads, fully validates, and applies filename. No daemon command is
// sent unless the whole document and every image path are valid.
func (l Loader) Load(filename string) error {
	if l.Client == nil || l.Commands == nil {
		return fmt.Errorf("configuration loader is not initialized")
	}
	filename, err := NormalizeFilename(filename)
	if err != nil {
		return err
	}
	data, err := os.ReadFile(filepath.Join(l.ConfigDir, filename))
	if err != nil {
		return fmt.Errorf("read configuration file: %w", err)
	}
	_, devices, reservedIDs, err := Parse(data)
	if err != nil {
		return err
	}
	for index, device := range devices {
		if name := device.GetParams()["file"]; name != "" {
			resolved, err := ResolveImagePath(l.ImageDir, name)
			if err != nil {
				return fmt.Errorf("device %d image path: %w", index, err)
			}
			device.Params["file"] = resolved
		}
	}
	if err := l.send("detach existing devices", l.Commands.DetachAll()); err != nil {
		return err
	}
	if err := l.send("restore reserved IDs", l.Commands.ReserveIDs(reservedIDs)); err != nil {
		return err
	}
	for _, device := range devices {
		if err := l.send(fmt.Sprintf("attach SCSI ID %d LUN %d", device.GetId(), device.GetUnit()), l.Commands.AttachDeviceDefinition(device)); err != nil {
			return err
		}
	}
	return nil
}

func (l Loader) send(action string, command *pb.PbCommand) error {
	result, err := l.Client.SendCommand(command)
	if err != nil {
		return fmt.Errorf("%s: %w", action, err)
	}
	if !result.GetStatus() {
		return fmt.Errorf("%s: %s", action, result.GetMsg())
	}
	return nil
}

// ResolveImagePath returns an absolute path below root. Existing absolute
// paths are accepted only when they are already below root.
func ResolveImagePath(root, name string) (string, error) {
	name = strings.TrimSpace(name)
	if name == "" {
		return "", fmt.Errorf("file name is required")
	}
	cleanName := filepath.Clean(filepath.FromSlash(name))
	if !filepath.IsAbs(cleanName) {
		return resolvePathWithin(root, name)
	}
	absoluteRoot, err := filepath.Abs(root)
	if err != nil {
		return "", fmt.Errorf("resolve root directory: %w", err)
	}
	relativeTarget, err := filepath.Rel(absoluteRoot, cleanName)
	if err != nil {
		return "", fmt.Errorf("resolve target path: %w", err)
	}
	if relativeTarget == ".." || strings.HasPrefix(relativeTarget, ".."+string(filepath.Separator)) {
		return "", fmt.Errorf("path escapes the configured directory")
	}
	return cleanName, nil
}

func resolvePathWithin(root, name string) (string, error) {
	relativeName := filepath.Clean(filepath.FromSlash(name))
	if relativeName == "." || filepath.IsAbs(relativeName) {
		return "", fmt.Errorf("invalid relative path")
	}
	absoluteRoot, err := filepath.Abs(root)
	if err != nil {
		return "", fmt.Errorf("resolve root directory: %w", err)
	}
	target := filepath.Join(absoluteRoot, relativeName)
	relativeTarget, err := filepath.Rel(absoluteRoot, target)
	if err != nil {
		return "", fmt.Errorf("resolve target path: %w", err)
	}
	if relativeTarget == ".." || strings.HasPrefix(relativeTarget, ".."+string(filepath.Separator)) {
		return "", fmt.Errorf("path escapes the configured directory")
	}
	return target, nil
}

func validFilename(filename string) bool {
	return filename != "" && !strings.Contains(filename, "..") && !strings.ContainsAny(filename, "/\\") && !strings.HasPrefix(filename, ".") && len(filename) <= 255
}

func stringPointer(value string) *string { return &value }
