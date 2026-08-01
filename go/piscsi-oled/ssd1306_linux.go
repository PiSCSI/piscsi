// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

//go:build linux

package oled

import (
	"fmt"

	"github.com/piscsi/piscsi/go/piscsi/i2c"
)

// pageWriteChunk bounds the longest normal-priority I2C transfer. At the
// usual 100 kHz bus rate, a 16-pixel data write completes in roughly 1.5 ms,
// allowing a queued PCA9554 input read to run before the next chunk.
const pageWriteChunk = 16

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
	config  SSD1306Config
	bus     *i2c.Bus
	ownsBus bool
}

func NewSSD1306(config SSD1306Config) (*SSD1306, error) {
	bus, err := i2c.Open(config.Device)
	if err != nil {
		return nil, err
	}
	display, err := NewSSD1306WithBus(config, bus)
	if err != nil {
		bus.Close()
		return nil, err
	}
	display.ownsBus = true
	return display, nil
}

// NewSSD1306WithBus reuses a bus shared with other I2C clients, such as the
// Control Board PCA9554. The caller retains ownership of bus.
func NewSSD1306WithBus(config SSD1306Config, bus *i2c.Bus) (*SSD1306, error) {
	if bus == nil {
		return nil, fmt.Errorf("I2C bus is required")
	}
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
	return &SSD1306{config: config, bus: bus}, nil
}

func (d *SSD1306) Init() error {
	comScan, segRemap := byte(0xc0), byte(0xa0)
	if d.config.Rotation == 180 {
		comScan, segRemap = 0xc8, 0xa1
	}
	multiplex := byte(0x1f)
	compins := byte(0x02)
	if d.config.Height == 64 {
		multiplex, compins = 0x3f, 0x12
	}
	if err := d.bus.Do(i2c.Normal, d.config.Address, func(transaction i2c.Transaction) error {
		return transaction.Write(commandData(0xae, 0xd5, 0x80, 0xa8, multiplex, 0xd3, 0x00, 0x40,
			0x8d, 0x14, 0x20, 0x02, segRemap, comScan, 0xda, compins, 0x81,
			d.config.Contrast, 0xd9, 0xf1, 0xdb, 0x40, 0xa4, 0xa6, 0xaf))
	}); err != nil {
		return fmt.Errorf("initialize SSD1306: %w", err)
	}
	return nil
}

func (d *SSD1306) Present(frame Frame) error {
	if !frame.valid() || frame.Height != d.config.Height {
		return fmt.Errorf("invalid %dx%d framebuffer", frame.Width, frame.Height)
	}
	for page := 0; page < frame.Height/8; page++ {
		if err := d.bus.Do(i2c.Normal, d.config.Address, func(transaction i2c.Transaction) error {
			return transaction.Write(commandData(byte(0xb0|page), 0x00, 0x10))
		}); err != nil {
			return fmt.Errorf("set page %d: %w", page, err)
		}
		pageData := ssd1306Page(frame, page)
		for offset := 1; offset < len(pageData); offset += pageWriteChunk {
			end := min(offset+pageWriteChunk, len(pageData))
			// Each I2C data transfer needs its own control byte. The SSD1306
			// column address remains valid across intervening PCA transactions.
			chunk := append([]byte{0x40}, pageData[offset:end]...)
			if err := d.bus.Do(i2c.Normal, d.config.Address, func(transaction i2c.Transaction) error {
				return transaction.Write(chunk)
			}); err != nil {
				return fmt.Errorf("write page %d data at column %d: %w", page, offset-1, err)
			}
		}
	}
	return nil
}

func (d *SSD1306) Clear() error {
	return d.Present(NewFrame(Width, d.config.Height))
}

func (d *SSD1306) Close() error {
	if d.ownsBus {
		return d.bus.Close()
	}
	return nil
}

func commandData(commands ...byte) []byte { return append([]byte{0x00}, commands...) }
