// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

var errNotRegularFile = errors.New("not a regular file")

// resolvePathWithin converts a browser-facing relative path into an absolute
// path below root. It rejects absolute paths and lexical traversal.
func resolvePathWithin(root, name string) (string, error) {
	name = strings.TrimSpace(name)
	if name == "" {
		return "", fmt.Errorf("file name is required")
	}

	relativeName := filepath.Clean(filepath.FromSlash(name))
	if relativeName == "." || filepath.IsAbs(relativeName) {
		return "", fmt.Errorf("invalid relative path")
	}

	absoluteRoot, err := filepath.Abs(root)
	if err != nil {
		return "", fmt.Errorf("resolve root directory: %w", err)
	}
	target := filepath.Join(absoluteRoot, relativeName)
	relativeTarget, err := filepath.Rel(absoluteRoot, target)
	if err != nil {
		return "", fmt.Errorf("resolve target path: %w", err)
	}
	if relativeTarget == ".." || strings.HasPrefix(relativeTarget, ".."+string(filepath.Separator)) {
		return "", fmt.Errorf("path escapes the configured directory")
	}
	return target, nil
}

// resolveImagePath converts an image name into an absolute path below the
// configured image directory. Absolute paths are accepted for compatibility
// with saved configurations, but only when they are already below root.
func resolveImagePath(root, name string) (string, error) {
	name = strings.TrimSpace(name)
	if name == "" {
		return "", fmt.Errorf("file name is required")
	}

	cleanName := filepath.Clean(filepath.FromSlash(name))
	if !filepath.IsAbs(cleanName) {
		return resolvePathWithin(root, name)
	}

	absoluteRoot, err := filepath.Abs(root)
	if err != nil {
		return "", fmt.Errorf("resolve root directory: %w", err)
	}
	relativeTarget, err := filepath.Rel(absoluteRoot, cleanName)
	if err != nil {
		return "", fmt.Errorf("resolve target path: %w", err)
	}
	if relativeTarget == ".." || strings.HasPrefix(relativeTarget, ".."+string(filepath.Separator)) {
		return "", fmt.Errorf("path escapes the configured directory")
	}
	return cleanName, nil
}

func uploadDestinationPath(root, subdirectory string) (string, error) {
	absoluteRoot, err := filepath.Abs(root)
	if err != nil {
		return "", fmt.Errorf("resolve root directory: %w", err)
	}
	target := absoluteRoot
	if strings.TrimSpace(subdirectory) != "" {
		target, err = resolvePathWithin(absoluteRoot, subdirectory)
		if err != nil {
			return "", err
		}
	}

	// Transfer destinations must already be directories. Resolve symlinks before
	// checking containment so an apparently safe subdirectory cannot redirect a
	// write outside the configured root.
	resolvedRoot, err := filepath.EvalSymlinks(absoluteRoot)
	if err != nil {
		return "", fmt.Errorf("resolve root directory symlinks: %w", err)
	}
	resolvedTarget, err := filepath.EvalSymlinks(target)
	if err != nil {
		return "", fmt.Errorf("resolve destination symlinks: %w", err)
	}
	relativeTarget, err := filepath.Rel(resolvedRoot, resolvedTarget)
	if err != nil {
		return "", fmt.Errorf("resolve destination path: %w", err)
	}
	if relativeTarget == ".." || strings.HasPrefix(relativeTarget, ".."+string(filepath.Separator)) {
		return "", fmt.Errorf("destination escapes the configured directory")
	}
	return target, nil
}

// openRegularFileWithin opens name beneath root without following a symlink
// outside root. Symlink leaves are rejected so callers do not serve a file
// selected by a locally planted link.
func openRegularFileWithin(root, name string) (*os.File, os.FileInfo, error) {
	rootHandle, err := os.OpenRoot(root)
	if err != nil {
		return nil, nil, fmt.Errorf("open root directory: %w", err)
	}
	defer rootHandle.Close()

	entryInfo, err := rootHandle.Lstat(name)
	if err != nil {
		return nil, nil, err
	}
	if !entryInfo.Mode().IsRegular() {
		return nil, nil, errNotRegularFile
	}

	file, err := rootHandle.Open(name)
	if err != nil {
		return nil, nil, err
	}
	info, err := file.Stat()
	if err != nil {
		file.Close()
		return nil, nil, err
	}
	if !info.Mode().IsRegular() {
		file.Close()
		return nil, nil, errNotRegularFile
	}

	return file, info, nil
}
