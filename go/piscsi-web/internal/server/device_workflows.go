// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"bytes"
	"encoding/json"
	"fmt"
	"math"
	"os"
	"strconv"
	"strings"

	pb "github.com/piscsi/piscsi/go/proto"
)

type attachmentIdentity struct {
	vendor    string
	product   string
	revision  string
	blockSize int32
}

type imageProperties struct {
	Vendor    *string `json:"vendor"`
	Product   string  `json:"product"`
	Revision  *string `json:"revision"`
	BlockSize any     `json:"block_size"`
}

func (s *Server) attachmentIdentity(imageName, driveName string, deviceType pb.PbDeviceType) (attachmentIdentity, error) {
	if driveName != "" {
		if s.driveProps == nil {
			return attachmentIdentity{}, fmt.Errorf("drive properties database not available")
		}
		drive, err := s.driveProps.GetByName(driveName)
		if err != nil {
			return attachmentIdentity{}, err
		}
		if drive.DeviceType != deviceType.String() {
			return attachmentIdentity{}, fmt.Errorf("drive profile %q is for %s, not %s", driveName, drive.DeviceType, deviceType)
		}
		blockSize, err := attachmentBlockSize(drive.BlockSize)
		if err != nil {
			return attachmentIdentity{}, fmt.Errorf("drive profile %q: %w", driveName, err)
		}
		return attachmentIdentity{
			vendor:    stringValue(drive.Vendor),
			product:   drive.Product,
			revision:  stringValue(drive.Revision),
			blockSize: blockSize,
		}, nil
	}

	if imageName == "" || s.config == nil || s.config.ConfigDir == "" {
		return attachmentIdentity{}, nil
	}
	propertiesPath, err := resolvePathWithin(s.config.ConfigDir, imageName+".properties")
	if err != nil {
		return attachmentIdentity{}, fmt.Errorf("invalid properties file path: %w", err)
	}
	data, err := os.ReadFile(propertiesPath)
	if os.IsNotExist(err) {
		return attachmentIdentity{}, nil
	}
	if err != nil {
		return attachmentIdentity{}, fmt.Errorf("read drive properties: %w", err)
	}

	var properties imageProperties
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.UseNumber()
	if err := decoder.Decode(&properties); err != nil {
		return attachmentIdentity{}, fmt.Errorf("parse drive properties %q: %w", imageName+".properties", err)
	}
	blockSize, err := attachmentBlockSize(properties.BlockSize)
	if err != nil {
		return attachmentIdentity{}, fmt.Errorf("drive properties %q: %w", imageName+".properties", err)
	}
	return attachmentIdentity{
		vendor:    stringValue(properties.Vendor),
		product:   properties.Product,
		revision:  stringValue(properties.Revision),
		blockSize: blockSize,
	}, nil
}

func attachmentBlockSize(value any) (int32, error) {
	if value == nil || value == "" {
		return 0, nil
	}

	var size int64
	switch value := value.(type) {
	case json.Number:
		parsed, err := value.Int64()
		if err != nil {
			return 0, fmt.Errorf("invalid block size %q", value)
		}
		size = parsed
	case float64:
		if math.Trunc(value) != value {
			return 0, fmt.Errorf("invalid block size %v", value)
		}
		size = int64(value)
	case int:
		size = int64(value)
	case int32:
		size = int64(value)
	case int64:
		size = value
	case string:
		parsed, err := strconv.ParseInt(value, 10, 32)
		if err != nil {
			return 0, fmt.Errorf("invalid block size %q", value)
		}
		size = parsed
	default:
		return 0, fmt.Errorf("invalid block size %v", value)
	}
	if size < 0 || size > math.MaxInt32 {
		return 0, fmt.Errorf("block size %d is out of range", size)
	}
	return int32(size), nil
}

func stringValue(value *string) string {
	if value == nil {
		return ""
	}
	return *value
}

func removableDeviceType(deviceType pb.PbDeviceType) bool {
	switch deviceType {
	case pb.PbDeviceType_SCRM, pb.PbDeviceType_SCMO, pb.PbDeviceType_SCCD, pb.PbDeviceType_SCTP:
		return true
	default:
		return false
	}
}

func parseDaynaPortProfile(value string) (string, string, error) {
	mode, interfaceName, found := strings.Cut(value, ":")
	if !found || mode == "" || interfaceName == "" || strings.Contains(interfaceName, ":") {
		return "", "", fmt.Errorf("invalid DaynaPort network profile")
	}
	if mode != "bridge" && mode != "proxyarp" {
		return "", "", fmt.Errorf("unsupported DaynaPort network mode %q", mode)
	}
	return mode, interfaceName, nil
}

// daynaPortProfileStatus validates the topology advertised by the daemon. The
// daemon is also responsible for the final host-side validation (notably the
// configured proxy-ARP uplink), so the web app never infers readiness from
// obsolete NAT, dhcpcd, or ifupdown files.
func daynaPortProfileStatus(mode string, networkInterface *pb.PbNetworkInterface) (bool, string) {
	if mode != "bridge" && mode != "proxyarp" {
		return false, fmt.Sprintf("Unsupported DaynaPort network mode %q", mode)
	}
	if networkInterface == nil {
		return false, "The selected network interface is not advertised by the PiSCSI daemon"
	}
	if !networkInterface.GetUp() {
		return false, fmt.Sprintf("Network interface %s is down", networkInterface.GetName())
	}
	for _, supportedMode := range networkInterface.GetSupportedMode() {
		if supportedMode != mode {
			continue
		}
		if mode == "bridge" {
			return true, "Wired bridge profile is active and ready"
		}
		return true, "Wi-Fi proxy-ARP profile is active and ready (IPv4 unicast and DHCP only)"
	}
	return false, fmt.Sprintf("Network interface %s does not support the DaynaPort %s profile", networkInterface.GetName(), mode)
}
