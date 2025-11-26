// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"fmt"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
)

const throttleHelpURL = "https://www.raspberrypi.com/documentation/computers/configuration.html#undervoltage-warning"

type throttleNotice struct {
	Category   string `json:"category"`
	Message    string `json:"message"`
	ReturnCode int    `json:"return_code"`
}

type serviceStatus struct {
	Netatalk bool
	Samba    bool
	FTP      bool
	Macproxy bool
	Webmin   bool
}

func (s *Server) runSystemCommand(name string, args ...string) ([]byte, error) {
	if s.systemCommand != nil {
		return s.systemCommand(name, args...)
	}
	return exec.Command(name, args...).CombinedOutput()
}

func (s *Server) systemHostname() string {
	if s.hostname != nil {
		return s.hostname()
	}
	hostname, err := os.Hostname()
	if err != nil {
		return "Unknown"
	}
	return hostname
}

func (s *Server) systemIPAddress() string {
	if s.hostIP != nil {
		return s.hostIP()
	}

	// A UDP connect does not send traffic, but asks the kernel which local
	// address it would use. This reports the PiSCSI host, not the browser.
	conn, err := net.Dial("udp", "10.255.255.255:1")
	if err != nil {
		return ""
	}
	defer conn.Close()
	host, _, err := net.SplitHostPort(conn.LocalAddr().String())
	if err != nil {
		return ""
	}
	return host
}

func (s *Server) companionServices() serviceStatus {
	output, err := s.runSystemCommand("ps", "-eo", "comm=,args=")
	if err != nil {
		if s.logger != nil {
			s.logger.Warn("Failed to inspect companion services", "error", err)
		}
		return serviceStatus{}
	}
	return parseServiceStatus(string(output))
}

func parseServiceStatus(processes string) serviceStatus {
	status := serviceStatus{}
	for _, line := range strings.Split(processes, "\n") {
		fields := strings.Fields(line)
		if len(fields) == 0 {
			continue
		}
		for _, field := range fields {
			name := filepath.Base(field)
			switch name {
			case "afpd":
				status.Netatalk = true
			case "smbd":
				status.Samba = true
			case "vsftpd":
				status.FTP = true
			case "macproxy", "macproxy.py":
				status.Macproxy = true
			case "miniserv.pl":
				status.Webmin = true
			}
		}
	}
	return status
}

func (s *Server) throttleNotices() []throttleNotice {
	output, err := s.runSystemCommand("vcgencmd", "get_throttled")
	if err != nil {
		return []throttleNotice{}
	}
	return parseThrottleNotices(string(output))
}

func parseThrottleNotices(output string) []throttleNotice {
	value := strings.TrimSpace(strings.TrimPrefix(strings.TrimSpace(output), "throttled="))
	bits, err := strconv.ParseUint(value, 0, 32)
	if err != nil {
		return []throttleNotice{}
	}

	notices := []throttleNotice{}
	if bits&1 != 0 {
		notices = append(notices, throttleNotice{
			Category:   "error",
			Message:    "Potential instability - Under voltage detected - Make sure to use a sufficient power source (2.5+ amps).",
			ReturnCode: 100,
		})
	}
	if bits&(1<<16) != 0 {
		notices = append(notices, throttleNotice{
			Category:   "warning",
			Message:    "Potential instability - Under voltage has occurred since last reboot.  Make sure to use a sufficient power source (2.5+ amps).",
			ReturnCode: 116,
		})
	}
	return notices
}

func runningEnvironment() string {
	hardware := readFirstSystemValue(
		"/proc/device-tree/model",
		"/sys/devices/virtual/dmi/id/product_name",
	)
	if vendor := readFirstSystemValue("/sys/devices/virtual/dmi/id/sys_vendor"); vendor != "" &&
		!strings.Contains(hardware, vendor) {
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

func readFirstSystemValue(paths ...string) string {
	for _, path := range paths {
		if data, err := os.ReadFile(path); err == nil {
			if value := strings.Trim(strings.TrimSpace(string(data)), "\x00"); value != "" {
				return value
			}
		}
	}
	return ""
}
