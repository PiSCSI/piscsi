// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

//go:build linux

package i2c

import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/unix"
)

const (
	i2cSlave = 0x0703
	i2cRDWR  = 0x0707
	i2cRead  = 0x0001
)

// Open opens device once and starts its serialized transaction worker.
func Open(device string) (*Bus, error) {
	if device == "" {
		device = "/dev/i2c-1"
	}
	fd, err := unix.Open(device, unix.O_RDWR|unix.O_CLOEXEC, 0)
	if err != nil {
		return nil, fmt.Errorf("open I2C device %s: %w", device, err)
	}
	return newBus(func(address int, fn func(Transaction) error) error {
		if _, _, errno := unix.Syscall(unix.SYS_IOCTL, uintptr(fd), uintptr(i2cSlave), uintptr(address)); errno != 0 {
			return fmt.Errorf("select I2C address %#x: %w", address, errno)
		}
		return fn(linuxTransaction{fd: fd, address: uint16(address)})
	}, func() error { return unix.Close(fd) }), nil
}

type linuxTransaction struct {
	fd      int
	address uint16
}

func (t linuxTransaction) Write(data []byte) error {
	for len(data) > 0 {
		n, err := unix.Write(t.fd, data)
		if err != nil {
			return err
		}
		if n == 0 {
			return unix.EIO
		}
		data = data[n:]
	}
	return nil
}

func (t linuxTransaction) Read(data []byte) error {
	for len(data) > 0 {
		n, err := unix.Read(t.fd, data)
		if err != nil {
			return err
		}
		if n == 0 {
			return unix.EIO
		}
		data = data[n:]
	}
	return nil
}

// WriteRead performs a combined transaction with a repeated START, matching
// SMBus read-byte-data semantics used by the legacy PCA9554 implementation.
func (t linuxTransaction) WriteRead(write, read []byte) error {
	if len(write) == 0 || len(read) == 0 {
		return fmt.Errorf("I2C write/read buffers are required")
	}
	messages := []i2cMessage{
		{address: t.address, length: uint16(len(write)), data: &write[0]},
		{address: t.address, flags: i2cRead, length: uint16(len(read)), data: &read[0]},
	}
	request := i2cRDWRRequest{messages: &messages[0], count: uint32(len(messages))}
	if _, _, errno := unix.Syscall(unix.SYS_IOCTL, uintptr(t.fd), uintptr(i2cRDWR), uintptr(unsafe.Pointer(&request))); errno != 0 {
		return errno
	}
	return nil
}

type i2cMessage struct {
	address uint16
	flags   uint16
	length  uint16
	data    *byte
}

type i2cRDWRRequest struct {
	messages *i2cMessage
	count    uint32
}
