// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

// Package hostinfo provides small, dependency-free host facts shared by local
// PiSCSI display clients.
package hostinfo

import (
	"fmt"
	"net"
	"os"
	"runtime"
	"strings"
)

// Network returns the hostname and the first non-loopback IPv4 address.
// An empty IP means no suitable local network address was available.
func Network() (ip, hostname string) {
	hostname, _ = os.Hostname()
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return "", hostname
	}
	for _, addr := range addrs {
		network, ok := addr.(*net.IPNet)
		if ok && !network.IP.IsLoopback() && network.IP.To4() != nil {
			return network.IP.String(), hostname
		}
	}
	return "", hostname
}

// Environment returns a compact, display-friendly host and OS description.
func Environment() string {
	hardware := firstSystemValue("/proc/device-tree/model", "/sys/devices/virtual/dmi/id/product_name")
	if vendor := firstSystemValue("/sys/devices/virtual/dmi/id/sys_vendor"); vendor != "" && !strings.Contains(hardware, vendor) {
		hardware = strings.TrimSpace(vendor + " " + hardware)
	}
	if hardware == "" {
		hardware = "Unknown Device"
	}
	osName := runtime.GOOS
	if data, err := os.ReadFile("/etc/os-release"); err == nil {
		for _, line := range strings.Split(string(data), "\n") {
			if strings.HasPrefix(line, "PRETTY_NAME=") {
				osName = strings.Trim(strings.TrimPrefix(line, "PRETTY_NAME="), `"'`)
				break
			}
		}
	}
	return fmt.Sprintf("%s, %s %s", hardware, osName, runtime.GOARCH)
}

func firstSystemValue(paths ...string) string {
	for _, path := range paths {
		if data, err := os.ReadFile(path); err == nil {
			if value := strings.Trim(strings.TrimSpace(string(data)), "\x00"); value != "" {
				return value
			}
		}
	}
	return ""
}
