// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

//go:build linux

package ctrlboard

import (
	"fmt"

	"github.com/piscsi/piscsi/go/piscsi/i2c"
)

// LinuxPCA9554 accesses the Control Board expander at its register interface.
// Its shared bus serializes I2C access with display writes.
type LinuxPCA9554 struct {
	bus     *i2c.Bus
	ownsBus bool
	address int
}

func OpenPCA9554(device string, address int) (*LinuxPCA9554, error) {
	bus, err := i2c.Open(device)
	if err != nil {
		return nil, err
	}
	pca, err := OpenPCA9554WithBus(bus, address)
	if err != nil {
		bus.Close()
		return nil, err
	}
	pca.ownsBus = true
	return pca, nil
}

// OpenPCA9554WithBus reuses a bus shared with the display. Input register
// reads use I2C input priority and can run before the next display page.
func OpenPCA9554WithBus(bus *i2c.Bus, address int) (*LinuxPCA9554, error) {
	if bus == nil {
		return nil, fmt.Errorf("I2C bus is required")
	}
	if address == 0 {
		address = 0x3f
	}
	return &LinuxPCA9554{bus: bus, address: address}, nil
}

func (p *LinuxPCA9554) ReadInput() (byte, error) { return p.readRegister(0) }

// ConfigurePins writes the PCA9554 configuration register (1=input,
// 0=output). The Control Board uses 0x2f: pins 0,1,2,3,5 inputs and 6,7 outputs.
func (p *LinuxPCA9554) ConfigurePins(configuration byte) error {
	return p.writeRegister(3, configuration)
}

func (p *LinuxPCA9554) Close() error {
	if p.ownsBus {
		return p.bus.Close()
	}
	return nil
}

func (p *LinuxPCA9554) readRegister(register byte) (byte, error) {
	data := []byte{0}
	if err := p.bus.Do(i2c.Input, p.address, func(transaction i2c.Transaction) error {
		return transaction.WriteRead([]byte{register}, data)
	}); err != nil {
		return 0, err
	}
	return data[0], nil
}

func (p *LinuxPCA9554) writeRegister(register, value byte) error {
	return p.bus.Do(i2c.Normal, p.address, func(transaction i2c.Transaction) error {
		return transaction.Write([]byte{register, value})
	})
}
