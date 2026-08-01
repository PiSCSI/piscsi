// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package oled

import (
	"context"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"strings"

	"github.com/piscsi/piscsi/go/piscsi"
	pb "github.com/piscsi/piscsi/go/proto"
)

const networkRefreshTicks = 10

// CommandSender is satisfied by the shared PiSCSI client and permits narrow
// fakes in monitor tests.
type CommandSender interface {
	SendCommand(*pb.PbCommand) (*pb.PbResult, error)
}

type networkInfoFunc func() (string, string)

// Monitor obtains daemon state, caches type metadata, and formats the display
// contract independently from the drawing code.
type Monitor struct {
	client       CommandSender
	commands     *piscsi.CommandBuilder
	removable    map[pb.PbDeviceType]bool
	network      networkInfoFunc
	ip, hostname string
	networkTicks int
}

func NewMonitor(client CommandSender, password string) *Monitor {
	commands := piscsi.NewCommandBuilder()
	commands.SetToken(password)
	return &Monitor{
		client: client, commands: commands, removable: make(map[pb.PbDeviceType]bool),
		network: localNetworkInfo, networkTicks: networkRefreshTicks,
	}
}

// LoadDeviceTypes is called once at startup. A failed metadata request does not
// prevent later status rendering: device-level properties remain a fallback.
func (m *Monitor) LoadDeviceTypes(ctx context.Context) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	result, err := m.client.SendCommand(m.commands.GetDeviceTypesInfo())
	if err != nil {
		return fmt.Errorf("get device types: %w", err)
	}
	if !result.GetStatus() {
		return fmt.Errorf("get device types: %s", result.GetMsg())
	}
	for _, entry := range result.GetDeviceTypesInfo().GetProperties() {
		m.removable[entry.GetType()] = entry.GetProperties().GetRemovable()
	}
	return nil
}

// Poll returns a fully formatted status screen. Context is checked before each
// request; the shared client supplies bounded connection and I/O timeouts.
func (m *Monitor) Poll(ctx context.Context) ([]string, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if m.networkTicks == networkRefreshTicks {
		m.ip, m.hostname = m.network()
		m.networkTicks = 0
	} else {
		m.networkTicks++
	}
	auth, err := m.client.SendCommand(m.commands.CheckAuthentication())
	if err != nil {
		return nil, fmt.Errorf("check authentication: %w", err)
	}
	if !auth.GetStatus() {
		return withNetwork([]string{"Permission denied!"}, m.ip, m.hostname), nil
	}
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	devices, err := m.client.SendCommand(m.commands.ListDevices())
	if err != nil {
		return nil, fmt.Errorf("list devices: %w", err)
	}
	if !devices.GetStatus() {
		return nil, fmt.Errorf("list devices: %s", devices.GetMsg())
	}
	return m.Format(devices.GetDevicesInfo().GetDevices()), nil
}

func (m *Monitor) Format(devices []*pb.PbDevice) []string {
	lines := make([]string, 0, len(devices)+1)
	if len(devices) == 0 {
		lines = append(lines, "No device attached!")
	} else {
		hasLUNs := false
		for _, device := range devices {
			if device.GetUnit() != 0 {
				hasLUNs = true
				break
			}
		}
		for _, device := range devices {
			line := []string{fmt.Sprint(device.GetId())}
			if hasLUNs {
				line = append(line, fmt.Sprint(device.GetUnit()))
			}
			line = append(line, typeSuffix(device.GetType()))
			filename := device.GetFile().GetName()
			if filename == "" {
				filename = device.GetParams()["file"]
			}
			if filename != "" {
				line = append(line, filepath.Base(filename))
			} else if m.removable[device.GetType()] || device.GetProperties().GetRemovable() {
				line = append(line, "[No Media]")
			}
			if vendor := strings.TrimSpace(device.GetVendor()); vendor != "" && vendor != "PiSCSI" {
				line = append(line, vendor)
				if product := strings.TrimSpace(device.GetProduct()); product != "" {
					line = append(line, product)
				}
			}
			lines = append(lines, strings.Join(line, " "))
		}
	}
	return withNetwork(lines, m.ip, m.hostname)
}

func typeSuffix(deviceType pb.PbDeviceType) string {
	name := deviceType.String()
	if len(name) >= 4 {
		return name[2:4]
	}
	return name
}

func withNetwork(lines []string, ip, hostname string) []string {
	if ip == "" {
		return append(lines, "No network connection")
	}
	return append(lines, fmt.Sprintf("IP %s - %s", ip, hostname))
}

func localNetworkInfo() (string, string) {
	hostname, _ := os.Hostname()
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return "", hostname
	}
	for _, addr := range addrs {
		ip, ok := addr.(*net.IPNet)
		if ok && !ip.IP.IsLoopback() && ip.IP.To4() != nil {
			return ip.IP.String(), hostname
		}
	}
	return "", hostname
}
