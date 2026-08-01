// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import (
	"bufio"
	"context"
	"fmt"
	"os"
	"runtime"
	"strconv"
	"strings"
	"syscall"

	"github.com/piscsi/piscsi/go/piscsi/hostinfo"
	pb "github.com/piscsi/piscsi/go/proto"
)

// SystemInfo contains the compact host and daemon facts displayed by the
// Control Board. Values are intentionally strings where a platform may not
// provide the corresponding metric.
type SystemInfo struct {
	Hostname       string
	IP             string
	DiskFreeMiB    uint64
	MemoryFreeMiB  uint64
	MemoryTotalMiB uint64
	CPUCount       int
	LoadAverage    string
	Version        string
	Environment    string
}

func (w *SCSIWorkflow) BuildSystemInfoMenu(ctx context.Context, slot SCSISlot, pageSize int) (*Menu, error) {
	if err := w.ready(ctx); err != nil {
		return nil, err
	}
	info := localSystemInfo(w.imageDir)
	result, err := w.client.SendCommand(w.commands.GetVersion())
	if err != nil {
		return nil, fmt.Errorf("get PiSCSI version: %w", err)
	}
	if !result.GetStatus() {
		return nil, resultError("get PiSCSI version", result)
	}
	info.Version = formatVersion(result.GetVersionInfo())
	return NewSystemInfoMenu(slot, info, pageSize)
}

func NewSystemInfoMenu(slot SCSISlot, info SystemInfo, pageSize int) (*Menu, error) {
	items := []MenuItem{{ID: "return", Label: "Return", Data: SlotAction{Kind: SlotActionReturn, Slot: slot}}}
	hostname := emptyAsDash(info.Hostname)
	items = append(items, MenuItem{ID: "host", Label: "[" + hostname + "]"})
	if info.IP == "" {
		items = append(items, MenuItem{ID: "ip", Label: "No network"})
	} else {
		items = append(items, MenuItem{ID: "ip", Label: "IP: " + info.IP})
	}
	if info.DiskFreeMiB > 0 {
		items = append(items, infoItem("disk", "Disk: %d MB free", info.DiskFreeMiB))
	} else {
		items = append(items, MenuItem{ID: "disk", Label: "Disk: unavailable"})
	}
	cpu := fmt.Sprintf("CPU: %d cores", info.CPUCount)
	if info.LoadAverage != "" {
		cpu += " load " + info.LoadAverage
	}
	items = append(items, MenuItem{ID: "cpu", Label: cpu})
	if info.MemoryTotalMiB > 0 {
		items = append(items, infoItem("memory", "Mem: %d/%d MB free", info.MemoryFreeMiB, info.MemoryTotalMiB))
	} else {
		items = append(items, MenuItem{ID: "memory", Label: "Mem: unavailable"})
	}
	items = append(items,
		MenuItem{ID: "version", Label: "PiSCSI v" + emptyAsDash(info.Version)},
		MenuItem{ID: "environment", Label: emptyAsDash(info.Environment)},
	)
	return NewMenu("System Info", items, pageSize)
}

func localSystemInfo(imageDir string) SystemInfo {
	ip, hostname := hostinfo.Network()
	free, _ := freeSpaceMiB(imageDir)
	memoryFree, memoryTotal := memoryMiB()
	return SystemInfo{
		Hostname: hostname, IP: ip, DiskFreeMiB: free,
		MemoryFreeMiB: memoryFree, MemoryTotalMiB: memoryTotal,
		CPUCount: runtime.NumCPU(), LoadAverage: loadAverage(), Environment: hostinfo.Environment(),
	}
}

func freeSpaceMiB(path string) (uint64, error) {
	if strings.TrimSpace(path) == "" {
		return 0, fmt.Errorf("image directory is required")
	}
	var stat syscall.Statfs_t
	if err := syscall.Statfs(path, &stat); err != nil {
		return 0, err
	}
	return stat.Bavail * uint64(stat.Bsize) / (1024 * 1024), nil
}

func memoryMiB() (available, total uint64) {
	file, err := os.Open("/proc/meminfo")
	if err != nil {
		return 0, 0
	}
	defer file.Close()
	for scanner := bufio.NewScanner(file); scanner.Scan(); {
		fields := strings.Fields(scanner.Text())
		if len(fields) < 2 {
			continue
		}
		value, err := strconv.ParseUint(fields[1], 10, 64)
		if err != nil {
			continue
		}
		switch fields[0] {
		case "MemTotal:":
			total = value / 1024
		case "MemAvailable:":
			available = value / 1024
		}
	}
	return available, total
}

func loadAverage() string {
	data, err := os.ReadFile("/proc/loadavg")
	if err != nil {
		return ""
	}
	fields := strings.Fields(string(data))
	if len(fields) == 0 {
		return ""
	}
	return fields[0]
}

func formatVersion(version *pb.PbVersionInfo) string {
	if version == nil {
		return ""
	}
	return fmt.Sprintf("%d.%d.%d", version.GetMajorVersion(), version.GetMinorVersion(), version.GetPatchVersion())
}
