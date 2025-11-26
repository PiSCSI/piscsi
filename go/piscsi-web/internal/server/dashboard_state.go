// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"fmt"
	"path/filepath"
	"strings"

	pb "github.com/piscsi/piscsi-web/proto"
	"golang.org/x/sys/unix"
)

func formatFileSize(size int64) string {
	const (
		kib = int64(1024)
		mib = 1024 * kib
	)
	switch {
	case size >= mib:
		return fmt.Sprintf("%.1f MiB", float64(size)/float64(mib))
	case size >= kib:
		return fmt.Sprintf("%.1f KiB", float64(size)/float64(kib))
	case size == 1:
		return "1 byte"
	default:
		return fmt.Sprintf("%d bytes", size)
	}
}

func freeDiskSpaceMiB(path string) (uint64, error) {
	var stats unix.Statfs_t
	if err := unix.Statfs(path, &stats); err != nil {
		return 0, err
	}
	return uint64(stats.Bavail) * uint64(stats.Bsize) / (1024 * 1024), nil
}

func imageNameRelativeTo(baseDir, name string) (string, bool) {
	if name == "" {
		return "", false
	}

	if filepath.IsAbs(name) {
		relative, err := filepath.Rel(baseDir, name)
		if err != nil || filepath.IsAbs(relative) {
			return "", false
		}
		name = relative
	}

	cleaned := filepath.ToSlash(filepath.Clean(name))
	if cleaned == ".." || strings.HasPrefix(cleaned, "../") {
		return "", false
	}
	return cleaned, true
}

func imageIsAttached(name string, attachedImages map[string]struct{}) bool {
	_, attached := attachedImages[filepath.ToSlash(filepath.Clean(name))]
	return attached
}

func validSCSIIDs(reserved map[int]struct{}, occupied []int) ([]int, int) {
	valid := make([]int, 0, 8-len(reserved))
	occupiedSet := make(map[int]struct{}, len(occupied))
	for _, id := range occupied {
		occupiedSet[id] = struct{}{}
	}

	recommended := -1
	for id := 7; id >= 0; id-- {
		if _, isReserved := reserved[id]; !isReserved {
			valid = append(valid, id)
			if _, isOccupied := occupiedSet[id]; !isOccupied && recommended == -1 {
				recommended = id
			}
		}
	}

	if recommended == -1 {
		if len(occupied) > 0 {
			recommended = occupied[0]
		} else {
			recommended = 0
		}
	}
	return valid, recommended
}

func reservedIDsFromServerInfoResult(result *pb.PbResult) ([]int32, bool) {
	if result == nil || !result.GetStatus() || result.GetServerInfo() == nil {
		return nil, false
	}
	return append([]int32(nil), result.GetServerInfo().GetReservedIdsInfo().GetIds()...), true
}
