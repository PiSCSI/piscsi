// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"net"

	pb "github.com/piscsi/piscsi-web/proto"
	"google.golang.org/protobuf/proto"
)

const (
	MagicWord = "RASCSI"
	Port      = 6868
)

func main() {
	listener, err := net.Listen("tcp", fmt.Sprintf(":%d", Port))
	if err != nil {
		log.Fatalf("Failed to listen on port %d: %v", Port, err)
	}
	defer listener.Close()

	log.Printf("Mock PiSCSI daemon listening on port %d", Port)
	log.Printf("Press Ctrl+C to stop")

	for {
		conn, err := listener.Accept()
		if err != nil {
			log.Printf("Failed to accept connection: %v", err)
			continue
		}

		go handleConnection(conn)
	}
}

func handleConnection(conn net.Conn) {
	defer conn.Close()

	// Read magic word (6 bytes)
	magic := make([]byte, 6)
	if _, err := io.ReadFull(conn, magic); err != nil {
		log.Printf("Failed to read magic word: %v", err)
		return
	}

	if string(magic) != MagicWord {
		log.Printf("Invalid magic word: %s", string(magic))
		return
	}

	// Read size header (4 bytes, little-endian)
	sizeHeader := make([]byte, 4)
	if _, err := io.ReadFull(conn, sizeHeader); err != nil {
		log.Printf("Failed to read size header: %v", err)
		return
	}

	requestSize := binary.LittleEndian.Uint32(sizeHeader)

	// Read the request payload
	requestData := make([]byte, requestSize)
	if _, err := io.ReadFull(conn, requestData); err != nil {
		log.Printf("Failed to read request payload: %v", err)
		return
	}

	// Unmarshal the request
	cmd := &pb.PbCommand{}
	if err := proto.Unmarshal(requestData, cmd); err != nil {
		log.Printf("Failed to unmarshal request: %v", err)
		return
	}

	log.Printf("Received command: %s", cmd.Operation.String())

	// Generate mock response based on operation
	result := generateMockResponse(cmd)

	// Marshal the response
	responseData, err := proto.Marshal(result)
	if err != nil {
		log.Printf("Failed to marshal response: %v", err)
		return
	}

	// Send response size (4 bytes, little-endian)
	responseSizeHeader := make([]byte, 4)
	binary.LittleEndian.PutUint32(responseSizeHeader, uint32(len(responseData)))
	if _, err := conn.Write(responseSizeHeader); err != nil {
		log.Printf("Failed to send response size: %v", err)
		return
	}

	// Send response payload
	if _, err := conn.Write(responseData); err != nil {
		log.Printf("Failed to send response payload: %v", err)
		return
	}

	log.Printf("Sent response for %s (%d bytes)", cmd.Operation.String(), len(responseData))
}

func generateMockResponse(cmd *pb.PbCommand) *pb.PbResult {
	result := &pb.PbResult{
		Status: true,
	}

	switch cmd.Operation {
	case pb.PbOperation_VERSION_INFO:
		result.Result = &pb.PbResult_VersionInfo{
			VersionInfo: getMockVersionInfo(),
		}

	case pb.PbOperation_SERVER_INFO:
		result.Result = &pb.PbResult_ServerInfo{
			ServerInfo: getMockServerInfo(),
		}

	case pb.PbOperation_DEVICES_INFO:
		result.Result = &pb.PbResult_DevicesInfo{
			DevicesInfo: getMockDevicesInfo(),
		}

	case pb.PbOperation_DEFAULT_IMAGE_FILES_INFO:
		result.Result = &pb.PbResult_ImageFilesInfo{
			ImageFilesInfo: getMockImageFilesInfo(),
		}

	case pb.PbOperation_DEVICE_TYPES_INFO:
		result.Result = &pb.PbResult_DeviceTypesInfo{
			DeviceTypesInfo: getMockDeviceTypesInfo(),
		}

	case pb.PbOperation_LOG_LEVEL_INFO:
		result.Result = &pb.PbResult_LogLevelInfo{
			LogLevelInfo: getMockLogLevelInfo(),
		}

	case pb.PbOperation_NETWORK_INTERFACES_INFO:
		result.Result = &pb.PbResult_NetworkInterfacesInfo{
			NetworkInterfacesInfo: getMockNetworkInfo(),
		}

	case pb.PbOperation_ATTACH:
		// Simulate successful attach
		result.Result = &pb.PbResult_DevicesInfo{
			DevicesInfo: getMockDevicesInfoWithAttached(cmd.Devices),
		}

	case pb.PbOperation_DETACH, pb.PbOperation_DETACH_ALL:
		// Return empty device list
		result.Result = &pb.PbResult_DevicesInfo{
			DevicesInfo: &pb.PbDevicesInfo{
				Devices: []*pb.PbDevice{},
			},
		}

	default:
		log.Printf("Unhandled operation: %s", cmd.Operation.String())
	}

	return result
}

func getMockVersionInfo() *pb.PbVersionInfo {
	return &pb.PbVersionInfo{
		MajorVersion: 24,
		MinorVersion: 3,
		PatchVersion: -1, // -1 indicates development version
	}
}

func getMockServerInfo() *pb.PbServerInfo {
	return &pb.PbServerInfo{
		VersionInfo:           getMockVersionInfo(),
		LogLevelInfo:          getMockLogLevelInfo(),
		ImageFilesInfo:        getMockImageFilesInfo(),
		MappingInfo:           getMockMappingInfo(),
		NetworkInterfacesInfo: getMockNetworkInfo(),
		DeviceTypesInfo:       getMockDeviceTypesInfo(),
		ReservedIdsInfo:       getMockReservedIdsInfo(),
		OperationInfo:         getMockOperationInfo(),
		DevicesInfo:           getMockDevicesInfo(),
	}
}

func getMockDevicesInfo() *pb.PbDevicesInfo {
	return &pb.PbDevicesInfo{
		Devices: []*pb.PbDevice{
			{
				Id:     0,
				Unit:   0,
				Type:   pb.PbDeviceType_SCHD,
				Properties: &pb.PbDeviceProperties{
					ReadOnly:          false,
					Protectable:       true,
					Stoppable:         true,
					Removable:         false,
					Lockable:          false,
					SupportsFile:      true,
					SupportsParams:    true,
					Luns:              32,
					BlockSizes:        []uint32{512, 1024, 2048, 4096},
				},
				Status: &pb.PbDeviceStatus{
					Protected: false,
					Stopped:   false,
					Removed:   false,
					Locked:    false,
				},
				File: &pb.PbImageFile{
					Name:     "test-disk.hda",
					Type:     pb.PbDeviceType_SCHD,
					Size:     10485760000, // ~10GB
					ReadOnly: false,
				},
				Vendor:     "PiSCSI",
				Product:    "MOCK DRIVE",
				Revision:   "1.0",
				BlockSize:  512,
				BlockCount: 20480000, // ~10GB
				Params: map[string]string{
					"file": "test-disk.hda",
				},
			},
		},
	}
}

func getMockDevicesInfoWithAttached(devices []*pb.PbDeviceDefinition) *pb.PbDevicesInfo {
	mockDevices := []*pb.PbDevice{}

	for _, dev := range devices {
		mockDevice := &pb.PbDevice{
			Id:   dev.Id,
			Unit: dev.Unit,
			Type: dev.Type,
			Properties: &pb.PbDeviceProperties{
				ReadOnly:       false,
				Protectable:    true,
				Stoppable:      true,
				Removable:      dev.Type == pb.PbDeviceType_SCCD || dev.Type == pb.PbDeviceType_SCRM,
				Lockable:       dev.Type == pb.PbDeviceType_SCCD || dev.Type == pb.PbDeviceType_SCRM,
				SupportsFile:   true,
				SupportsParams: true,
				Luns:           32,
				BlockSizes:     []uint32{512, 1024, 2048, 4096},
			},
			Status: &pb.PbDeviceStatus{
				Protected: false,
				Stopped:   false,
				Removed:   false,
				Locked:    false,
			},
			BlockSize:  512,
			BlockCount: 20480000,
		}

		if dev.Params != nil {
			if file, ok := dev.Params["file"]; ok {
				mockDevice.File = &pb.PbImageFile{
					Name:     file,
					Type:     dev.Type,
					Size:     10485760000,
					ReadOnly: false,
				}
				if mockDevice.Params == nil {
					mockDevice.Params = make(map[string]string)
				}
				mockDevice.Params["file"] = file
			}
		}

		if dev.Vendor != "" {
			mockDevice.Vendor = dev.Vendor
		} else {
			mockDevice.Vendor = "PiSCSI"
		}

		if dev.Product != "" {
			mockDevice.Product = dev.Product
		} else {
			mockDevice.Product = "MOCK DRIVE"
		}

		if dev.Revision != "" {
			mockDevice.Revision = dev.Revision
		} else {
			mockDevice.Revision = "1.0"
		}

		mockDevices = append(mockDevices, mockDevice)
	}

	return &pb.PbDevicesInfo{
		Devices: mockDevices,
	}
}

func getMockImageFilesInfo() *pb.PbImageFilesInfo {
	return &pb.PbImageFilesInfo{
		DefaultImageFolder: "/home/pi/images",
		ImageFiles: []*pb.PbImageFile{
			{
				Name:     "system.hda",
				Type:     pb.PbDeviceType_SCHD,
				Size:     10737418240, // 10GB
				ReadOnly: false,
			},
			{
				Name:     "data.hda",
				Type:     pb.PbDeviceType_SCHD,
				Size:     5368709120, // 5GB
				ReadOnly: false,
			},
			{
				Name:     "cdrom.iso",
				Type:     pb.PbDeviceType_SCCD,
				Size:     734003200, // ~700MB
				ReadOnly: true,
			},
		},
		Depth: 1,
	}
}

func getMockDeviceTypesInfo() *pb.PbDeviceTypesInfo {
	return &pb.PbDeviceTypesInfo{
		Properties: []*pb.PbDeviceTypeProperties{
			{
				Type: pb.PbDeviceType_SCHD,
				Properties: &pb.PbDeviceProperties{
					ReadOnly:       false,
					Protectable:    true,
					Stoppable:      true,
					Removable:      false,
					Lockable:       false,
					SupportsFile:   true,
					SupportsParams: true,
					Luns:           32,
					BlockSizes:     []uint32{512, 1024, 2048, 4096},
				},
			},
			{
				Type: pb.PbDeviceType_SCCD,
				Properties: &pb.PbDeviceProperties{
					ReadOnly:       true,
					Protectable:    false,
					Stoppable:      true,
					Removable:      true,
					Lockable:       true,
					SupportsFile:   true,
					SupportsParams: true,
					Luns:           32,
					BlockSizes:     []uint32{2048},
				},
			},
			{
				Type: pb.PbDeviceType_SCRM,
				Properties: &pb.PbDeviceProperties{
					ReadOnly:       false,
					Protectable:    true,
					Stoppable:      true,
					Removable:      true,
					Lockable:       true,
					SupportsFile:   true,
					SupportsParams: true,
					Luns:           32,
					BlockSizes:     []uint32{512, 1024, 2048},
				},
			},
		},
	}
}

func getMockLogLevelInfo() *pb.PbLogLevelInfo {
	return &pb.PbLogLevelInfo{
		CurrentLogLevel: "info",
		LogLevels:       []string{"trace", "debug", "info", "warn", "error", "off"},
	}
}

func getMockNetworkInfo() *pb.PbNetworkInterfacesInfo {
	return &pb.PbNetworkInterfacesInfo{
		Name: []string{"eth0", "wlan0"},
	}
}

func getMockMappingInfo() *pb.PbMappingInfo {
	return &pb.PbMappingInfo{
		Mapping: map[string]pb.PbDeviceType{
			"hda":   pb.PbDeviceType_SCHD,
			"hds":   pb.PbDeviceType_SCHD,
			"hdr":   pb.PbDeviceType_SCRM,
			"iso":   pb.PbDeviceType_SCCD,
			"cdr":   pb.PbDeviceType_SCCD,
			"toast": pb.PbDeviceType_SCCD,
		},
	}
}

func getMockReservedIdsInfo() *pb.PbReservedIdsInfo {
	return &pb.PbReservedIdsInfo{
		Ids: []int32{7}, // ID 7 is typically reserved for the initiator
	}
}

func getMockOperationInfo() *pb.PbOperationInfo {
	return &pb.PbOperationInfo{
		Operations: map[int32]*pb.PbOperationMetaData{
			int32(pb.PbOperation_ATTACH): {
				ServerSideName: "ATTACH",
				Description:    "Attach a device",
			},
			int32(pb.PbOperation_DETACH): {
				ServerSideName: "DETACH",
				Description:    "Detach a device",
			},
			int32(pb.PbOperation_DEVICES_INFO): {
				ServerSideName: "DEVICES_INFO",
				Description:    "Get device information",
			},
			int32(pb.PbOperation_SERVER_INFO): {
				ServerSideName: "SERVER_INFO",
				Description:    "Get server information",
			},
		},
	}
}
