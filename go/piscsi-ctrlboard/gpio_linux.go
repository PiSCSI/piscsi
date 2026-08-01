// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

//go:build linux

package ctrlboard

import (
	"context"
	"fmt"
	"time"
	"unsafe"

	"golang.org/x/sys/unix"
)

const (
	gpioGetLineEventIOCTL = 0xc030b404 // GPIO_GET_LINEEVENT_IOCTL (v1 UAPI)
	gpioHandleInput       = 0x1
	// GPIOEVENT_REQUEST_RISING_EDGE is bit 0; the PCA9554 INT output is
	// active-low, so its asserted interrupt is GPIOEVENT_REQUEST_FALLING_EDGE
	// at bit 1.
	gpioFallingEdge = 0x2
)

// gpioEventRequest is the Linux GPIO v1 character-device request structure.
// Pi OS releases exposing the v2 API retain v1 compatibility; using this
// stable UAPI keeps the Control Board build CGO-free on ARMv7 and ARM64.
type gpioEventRequest struct {
	lineOffset    uint32
	handleFlags   uint32
	eventFlags    uint32
	consumerLabel [32]byte
	fd            int32
}

// LinuxEdgeSource subscribes to falling edges on a BCM GPIO line, normally
// line 9 of /dev/gpiochip0 for the PCA9554 interrupt pin.
type LinuxEdgeSource struct {
	chipFD  int
	eventFD int
}

func OpenFallingEdgeSource(chip string, line int) (*LinuxEdgeSource, error) {
	if chip == "" {
		chip = "/dev/gpiochip0"
	}
	if line < 0 {
		return nil, fmt.Errorf("invalid GPIO line %d", line)
	}
	chipFD, err := unix.Open(chip, unix.O_RDONLY|unix.O_CLOEXEC, 0)
	if err != nil {
		return nil, fmt.Errorf("open GPIO chip %s: %w", chip, err)
	}
	request := gpioEventRequest{lineOffset: uint32(line), handleFlags: gpioHandleInput, eventFlags: gpioFallingEdge, fd: -1}
	copy(request.consumerLabel[:], "piscsi-ctrlboard")
	if _, _, errno := unix.Syscall(unix.SYS_IOCTL, uintptr(chipFD), uintptr(gpioGetLineEventIOCTL), uintptr(unsafe.Pointer(&request))); errno != 0 {
		unix.Close(chipFD)
		return nil, fmt.Errorf("request GPIO %d falling edges: %w", line, errno)
	}
	return &LinuxEdgeSource{chipFD: chipFD, eventFD: int(request.fd)}, nil
}

func (s *LinuxEdgeSource) Run(ctx context.Context, callback func(time.Time)) error {
	if s == nil || s.eventFD < 0 {
		return fmt.Errorf("GPIO edge source is closed")
	}
	fd := []unix.PollFd{{Fd: int32(s.eventFD), Events: unix.POLLIN}}
	data := make([]byte, 16) // struct gpioevent_data
	for {
		if err := ctx.Err(); err != nil {
			return nil
		}
		ready, err := unix.Poll(fd, 100)
		if err != nil {
			if err == unix.EINTR {
				continue
			}
			return fmt.Errorf("wait for GPIO edge: %w", err)
		}
		if ready == 0 {
			continue
		}
		if _, err := unix.Read(s.eventFD, data); err != nil {
			return fmt.Errorf("read GPIO edge: %w", err)
		}
		// The kernel timestamp is monotonic while the latency meter uses the
		// time received by this process. That is the earliest comparable point
		// before its I2C read and menu-state update.
		callback(time.Now())
	}
}

func (s *LinuxEdgeSource) Close() error {
	if s == nil {
		return nil
	}
	var err error
	if s.eventFD >= 0 {
		err = unix.Close(s.eventFD)
		s.eventFD = -1
	}
	if s.chipFD >= 0 {
		if closeErr := unix.Close(s.chipFD); err == nil {
			err = closeErr
		}
		s.chipFD = -1
	}
	return err
}
