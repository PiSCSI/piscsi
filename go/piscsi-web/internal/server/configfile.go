// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"

	"github.com/gin-gonic/gin"
	pb "github.com/piscsi/piscsi/go/proto"
)

const (
	configFileSuffix      = ".json"
	defaultConfigFilename = "default.json"
)

// savedConfiguration uses the current object-format JSON shape. Reservation
// entries intentionally omit the Python web client's memo field.
type savedConfiguration struct {
	Version     string             `json:"version"`
	Devices     []savedDevice      `json:"devices"`
	ReservedIDs []savedReservation `json:"reserved_ids"`
}

type savedDevice struct {
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

type savedReservation struct {
	ID int32 `json:"id"`
}

func normalizeConfigFilename(name string) (string, error) {
	name = strings.TrimSpace(name)
	if name == "" {
		return "", fmt.Errorf("file name is required")
	}
	if filepath.Base(name) != name || !isValidFilename(name) {
		return "", fmt.Errorf("invalid configuration file name")
	}
	if filepath.Ext(name) == "" {
		name += configFileSuffix
	}
	if !strings.EqualFold(filepath.Ext(name), configFileSuffix) {
		return "", fmt.Errorf("configuration files must use the %s suffix", configFileSuffix)
	}
	return name, nil
}

func marshalConfiguration(info *pb.PbServerInfo) ([]byte, error) {
	if info == nil {
		return nil, fmt.Errorf("PiSCSI server information is missing")
	}

	versionInfo := info.GetVersionInfo()
	config := savedConfiguration{
		Version: fmt.Sprintf("%d.%d.%d",
			versionInfo.GetMajorVersion(),
			versionInfo.GetMinorVersion(),
			versionInfo.GetPatchVersion(),
		),
		Devices:     make([]savedDevice, 0),
		ReservedIDs: make([]savedReservation, 0),
	}

	for _, device := range info.GetDevicesInfo().GetDevices() {
		params := make(map[string]string, len(device.GetParams()))
		for key, value := range device.GetParams() {
			params[key] = value
		}

		saved := savedDevice{
			ID:         device.GetId(),
			Unit:       device.GetUnit(),
			DeviceType: device.GetType().String(),
			Params:     params,
			Protected:  device.GetStatus().GetProtected(),
		}

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
		config.ReservedIDs = append(config.ReservedIDs, savedReservation{ID: id})
	}

	data, err := json.MarshalIndent(config, "", "    ")
	if err != nil {
		return nil, fmt.Errorf("encode configuration: %w", err)
	}
	return append(data, '\n'), nil
}

func parseConfiguration(data []byte) (*savedConfiguration, []*pb.PbDeviceDefinition, []int32, error) {
	trimmed := bytes.TrimSpace(data)
	if len(trimmed) > 0 && trimmed[0] == '[' {
		return nil, nil, nil, fmt.Errorf("legacy top-level-list configurations are not supported; load and re-save the file with the Python web client to migrate it")
	}

	decoder := json.NewDecoder(bytes.NewReader(data))
	var config savedConfiguration
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
		if saved.Unit < 0 || saved.Unit > 31 {
			return nil, nil, nil, fmt.Errorf("device %d has invalid LUN %d", index, saved.Unit)
		}
		location := [2]int32{saved.ID, saved.Unit}
		if _, exists := occupied[location]; exists {
			return nil, nil, nil, fmt.Errorf("duplicate device at SCSI ID %d LUN %d", saved.ID, saved.Unit)
		}
		occupied[location] = struct{}{}
		usedIDs[saved.ID] = struct{}{}

		typeValue, exists := pb.PbDeviceType_value[strings.ToUpper(saved.DeviceType)]
		if !exists || pb.PbDeviceType(typeValue) == pb.PbDeviceType_UNDEFINED {
			return nil, nil, nil, fmt.Errorf("device %d has invalid type %q", index, saved.DeviceType)
		}

		params := make(map[string]string, len(saved.Params)+1)
		for key, value := range saved.Params {
			params[key] = value
		}
		if saved.Image != nil && *saved.Image != "" {
			params["file"] = *saved.Image
		}

		definition := &pb.PbDeviceDefinition{
			Id:        saved.ID,
			Unit:      saved.Unit,
			Type:      pb.PbDeviceType(typeValue),
			Params:    params,
			Protected: saved.Protected,
		}
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

func (s *Server) loadConfigurationFile(c *gin.Context, filename string) error {
	fullPath := filepath.Join(s.config.ConfigDir, filename)
	data, err := os.ReadFile(fullPath)
	if err != nil {
		return fmt.Errorf("read configuration file: %w", err)
	}

	// Parse and validate every entry before making the destructive DETACH_ALL
	// request.
	_, devices, reservedIDs, err := parseConfiguration(data)
	if err != nil {
		return err
	}
	for index, device := range devices {
		if filename := device.GetParams()["file"]; filename != "" {
			resolved, err := resolveImagePath(s.config.BaseDir, filename)
			if err != nil {
				return fmt.Errorf("device %d image path: %w", index, err)
			}
			device.Params["file"] = resolved
		}
	}

	builder := s.getCommandBuilder(c)
	if err := s.sendConfigurationCommand("detach existing devices", builder.DetachAll()); err != nil {
		return err
	}
	if err := s.sendConfigurationCommand("restore reserved IDs", builder.ReserveIDs(reservedIDs)); err != nil {
		return err
	}
	for _, device := range devices {
		description := fmt.Sprintf("attach SCSI ID %d LUN %d", device.GetId(), device.GetUnit())
		if err := s.sendConfigurationCommand(description, builder.AttachDeviceDefinition(device)); err != nil {
			return err
		}
	}
	return nil
}

// loadDefaultConfiguration loads default.json when it exists. Missing default
// configuration is normal and does not issue any daemon commands.
func (s *Server) loadDefaultConfiguration() (bool, error) {
	path := filepath.Join(s.config.ConfigDir, defaultConfigFilename)
	if _, err := os.Stat(path); err != nil {
		if os.IsNotExist(err) {
			return false, nil
		}
		return false, fmt.Errorf("inspect default configuration: %w", err)
	}
	if err := s.loadConfigurationFile(nil, defaultConfigFilename); err != nil {
		return false, err
	}
	return true, nil
}

func (s *Server) sendConfigurationCommand(action string, command *pb.PbCommand) error {
	result, err := s.piscsiClient.SendCommand(command)
	if err != nil {
		return fmt.Errorf("%s: %w", action, err)
	}
	if !result.GetStatus() {
		return fmt.Errorf("%s: %s", action, result.GetMsg())
	}
	return nil
}

func stringPointer(value string) *string {
	return &value
}
