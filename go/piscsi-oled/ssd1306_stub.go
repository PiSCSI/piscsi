// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

//go:build !linux

package oled

import "fmt"

type SSD1306Config struct {
	Device   string
	Address  int
	Height   int
	Rotation int
	Contrast byte
}

type SSD1306 struct{}

func NewSSD1306(SSD1306Config) (*SSD1306, error) {
	return nil, fmt.Errorf("SSD1306 I2C transport is only available on Linux")
}

func (d *SSD1306) Init() error { return fmt.Errorf("SSD1306 I2C transport is only available on Linux") }
func (d *SSD1306) Present(Frame) error {
	return fmt.Errorf("SSD1306 I2C transport is only available on Linux")
}
func (d *SSD1306) Clear() error {
	return fmt.Errorf("SSD1306 I2C transport is only available on Linux")
}
func (d *SSD1306) Close() error { return nil }
