// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

//go:build !linux

package ctrlboard

import (
	"fmt"

	"github.com/piscsi/piscsi/go/piscsi/i2c"
)

type LinuxPCA9554 struct{}

func OpenPCA9554(string, int) (*LinuxPCA9554, error) {
	return nil, fmt.Errorf("PCA9554 is supported only on Linux")
}

func OpenPCA9554WithBus(*i2c.Bus, int) (*LinuxPCA9554, error) {
	return nil, fmt.Errorf("PCA9554 is supported only on Linux")
}

func (*LinuxPCA9554) ReadInput() (byte, error) {
	return 0, fmt.Errorf("PCA9554 is supported only on Linux")
}

func (*LinuxPCA9554) ConfigurePins(byte) error {
	return fmt.Errorf("PCA9554 is supported only on Linux")
}

func (*LinuxPCA9554) Close() error { return nil }
