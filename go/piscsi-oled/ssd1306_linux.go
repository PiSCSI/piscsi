// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

//go:build linux

package oled

import (
	"fmt"
	"sync"

	"golang.org/x/sys/unix"
)

const i2cSlave = 0x0703

// SSD1306Config describes an I2C-connected 128-pixel-wide SSD1306 panel.
type SSD1306Config struct {
	Device   string
	Address  int
	Height   int
	Rotation int
	Contrast byte
}

// SSD1306 is a direct Linux I2C implementation. It intentionally uses no
// GPIO or display framework dependency.
type SSD1306 struct {
	config SSD1306Config
	fd     int
	mu     sync.Mutex
}

func NewSSD1306(config SSD1306Config) (*SSD1306, error) {
	if config.Device == "" {
		config.Device = "/dev/i2c-1"
	}
	if config.Address == 0 {
		config.Address = 0x3c
	}
	if config.Height != 32 && config.Height != 64 {
		return nil, fmt.Errorf("unsupported display height %d", config.Height)
	}
	if config.Rotation != 0 && config.Rotation != 180 {
		return nil, fmt.Errorf("unsupported rotation %d", config.Rotation)
	}
	if config.Contrast == 0 {
		config.Contrast = 0x7f
	}
	return &SSD1306{config: config, fd: -1}, nil
}

func (d *SSD1306) Init() error {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.fd >= 0 {
		return nil
	}
	fd, err := unix.Open(d.config.Device, unix.O_RDWR|unix.O_CLOEXEC, 0)
	if err != nil {
		return fmt.Errorf("open I2C device %s: %w", d.config.Device, err)
	}
	if _, _, errno := unix.Syscall(unix.SYS_IOCTL, uintptr(fd), uintptr(i2cSlave), uintptr(d.config.Address)); errno != 0 {
		unix.Close(fd)
		return fmt.Errorf("select I2C address %#x: %w", d.config.Address, errno)
	}
	d.fd = fd
	comScan, segRemap := byte(0xc0), byte(0xa0)
	if d.config.Rotation == 180 {
		comScan, segRemap = 0xc8, 0xa1
	}
	multiplex := byte(0x1f)
	compins := byte(0x02)
	if d.config.Height == 64 {
		multiplex, compins = 0x3f, 0x12
	}
	if err := d.command(0xae, 0xd5, 0x80, 0xa8, multiplex, 0xd3, 0x00, 0x40,
		0x8d, 0x14, 0x20, 0x02, segRemap, comScan, 0xda, compins, 0x81,
		d.config.Contrast, 0xd9, 0xf1, 0xdb, 0x40, 0xa4, 0xa6, 0xaf); err != nil {
		unix.Close(d.fd)
		d.fd = -1
		return fmt.Errorf("initialize SSD1306: %w", err)
	}
	return nil
}

func (d *SSD1306) Present(frame Frame) error {
	if !frame.valid() || frame.Height != d.config.Height {
		return fmt.Errorf("invalid %dx%d framebuffer", frame.Width, frame.Height)
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.fd < 0 {
		return fmt.Errorf("SSD1306 is not initialized")
	}
	for page := 0; page < frame.Height/8; page++ {
		if err := d.command(byte(0xb0|page), 0x00, 0x10); err != nil {
			return fmt.Errorf("set page %d: %w", page, err)
		}
		if err := d.write(ssd1306Page(frame, page)); err != nil {
			return fmt.Errorf("write page %d: %w", page, err)
		}
	}
	return nil
}

func (d *SSD1306) Clear() error {
	return d.Present(NewFrame(Width, d.config.Height))
}

func (d *SSD1306) Close() error {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.fd < 0 {
		return nil
	}
	err := unix.Close(d.fd)
	d.fd = -1
	return err
}

func (d *SSD1306) command(commands ...byte) error {
	return d.write(append([]byte{0x00}, commands...))
}

func (d *SSD1306) write(data []byte) error {
	for len(data) > 0 {
		n, err := unix.Write(d.fd, data)
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
