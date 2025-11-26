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

	pb "github.com/piscsi/piscsi-web/proto"
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

var bridgeConfigurationPaths = struct {
	sysctl string
	nat    string
	dhcpcd string
	bridge string
}{
	sysctl: "/etc/sysctl.conf",
	nat:    "/etc/iptables/rules.v4",
	dhcpcd: "/etc/dhcpcd.conf",
	bridge: "/etc/network/interfaces.d/piscsi_bridge",
}

func bridgeConfigurationStatus(interfaceName string) (bool, string) {
	switch {
	case interfaceName == "piscsi_bridge":
		return true, "The piscsi_bridge network bridge is active and ready"
	case strings.HasPrefix(interfaceName, "wlan"), strings.HasPrefix(interfaceName, "wlx"):
		missing := []string{}
		if !fileContainsLine(bridgeConfigurationPaths.sysctl, "net.ipv4.ip_forward=1") {
			missing = append(missing, "IPv4 forwarding")
		}
		if _, err := os.Stat(bridgeConfigurationPaths.nat); err != nil {
			missing = append(missing, "NAT")
		}
		if len(missing) != 0 {
			return false, fmt.Sprintf("Configure the network bridge for %s first: %s", interfaceName, strings.Join(missing, ", "))
		}
		return true, fmt.Sprintf("Wireless network bridge enabled for %s", interfaceName)
	case strings.HasPrefix(interfaceName, "eth"), strings.HasPrefix(interfaceName, "enx"), strings.HasPrefix(interfaceName, "enp"):
		missing := []string{}
		if !fileContainsLine(bridgeConfigurationPaths.dhcpcd, "denyinterfaces "+interfaceName) {
			missing = append(missing, bridgeConfigurationPaths.dhcpcd)
		}
		if _, err := os.Stat(bridgeConfigurationPaths.bridge); err != nil {
			missing = append(missing, bridgeConfigurationPaths.bridge)
		}
		if len(missing) != 0 {
			return false, fmt.Sprintf("Configure the network bridge for %s first: %s", interfaceName, strings.Join(missing, ", "))
		}
		return true, fmt.Sprintf("Wired network bridge enabled for %s", interfaceName)
	default:
		return false, fmt.Sprintf("Unable to validate the network bridge for interface %s", interfaceName)
	}
}

func fileContainsLine(path, expected string) bool {
	data, err := os.ReadFile(path)
	if err != nil {
		return false
	}
	for _, line := range strings.Split(string(data), "\n") {
		if strings.TrimSpace(line) == expected {
			return true
		}
	}
	return false
}
