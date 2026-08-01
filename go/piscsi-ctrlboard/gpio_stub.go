// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

//go:build !linux

package ctrlboard

import (
	"context"
	"fmt"
	"time"
)

type LinuxEdgeSource struct{}

func OpenFallingEdgeSource(string, int) (*LinuxEdgeSource, error) {
	return nil, fmt.Errorf("GPIO edge events are supported only on Linux")
}

func (*LinuxEdgeSource) Run(context.Context, func(time.Time)) error {
	return fmt.Errorf("GPIO edge events are supported only on Linux")
}

func (*LinuxEdgeSource) Close() error { return nil }
