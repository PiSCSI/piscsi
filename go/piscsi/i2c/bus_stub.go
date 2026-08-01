// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

//go:build !linux

package i2c

import "fmt"

func Open(string) (*Bus, error) { return nil, fmt.Errorf("I2C is supported only on Linux") }
