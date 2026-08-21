package piscsi

import "fmt"

const Copyright = "Copyright (C) 2020-2026 Contributors to the PiSCSI project"

// VersionBanner returns the common version information for PiSCSI applications.
//
//go:generate ./generate_version.sh
func VersionBanner(app string) string {
	return fmt.Sprintf("%s\nVersion %s\n%s\n", app, ProjectVersion, Copyright)
}
