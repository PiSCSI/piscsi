// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package hostinfo

import "testing"

func TestNetworkAlwaysReturnsHostnameWhenAvailable(t *testing.T) {
	_, hostname := Network()
	if hostname == "" {
		t.Fatal("Network returned an empty hostname")
	}
}

func TestEnvironmentIsDisplaySafe(t *testing.T) {
	if environment := Environment(); environment == "" {
		t.Fatal("Environment returned an empty description")
	}
}
