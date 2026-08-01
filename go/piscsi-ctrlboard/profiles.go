// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package ctrlboard

import "github.com/piscsi/piscsi/go/piscsi/configuration"

// LoadProfile delegates to the shared object-style configuration loader so
// Control Board and web profile behavior cannot diverge.
func LoadProfile(loader configuration.Loader, filename string) error { return loader.Load(filename) }
