// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/piscsi/piscsi/go/piscsi-web/internal/driveprops"
	pb "github.com/piscsi/piscsi/go/proto"
)

type propertyMetadata struct {
	Name  string
	Value interface{}
}

func drivePresetTemplateData(drives []driveprops.DriveProperty) map[string][]map[string]interface{} {
	data := map[string][]map[string]interface{}{
		"HardDrives":      {},
		"CDROMDrives":     {},
		"RemovableDrives": {},
		"TapeDrives":      {},
	}
	for _, drive := range drives {
		preset := map[string]interface{}{
			"Name":        drive.Name,
			"Description": drive.Description,
			"URL":         drive.URL,
			"SecureName":  strings.ReplaceAll(drive.Name, " ", "_"),
		}
		if drive.FileType != nil {
			preset["FileType"] = *drive.FileType
		}
		if drive.Size != nil && *drive.Size > 0 {
			preset["SizeMB"] = fmt.Sprintf("%.2f", float64(*drive.Size)/(1024*1024))
		}

		var category string
		switch drive.DeviceType {
		case "SCHD":
			category = "HardDrives"
		case "SCCD":
			category = "CDROMDrives"
		case "SCRM":
			category = "RemovableDrives"
		case "SCTP":
			category = "TapeDrives"
		default:
			continue
		}
		data[category] = append(data[category], preset)
	}
	for category := range data {
		sort.Slice(data[category], func(i, j int) bool {
			return data[category][i]["Name"].(string) < data[category][j]["Name"].(string)
		})
	}
	return data
}

func compatibleCDImages(info *pb.PbImageFilesInfo) []string {
	images := make([]string, 0)
	if info == nil {
		return images
	}
	for _, image := range info.GetImageFiles() {
		if image.GetType() == pb.PbDeviceType_SCCD {
			images = append(images, image.GetName())
		}
	}
	sort.Strings(images)
	return images
}

// readImageProperties returns all metadata stored in the properties file that
// matches an image. Properties files mirror the image's relative path below
// the configured properties/config directory.
func (s *Server) readImageProperties(imageName string) ([]propertyMetadata, bool) {
	if s.config == nil || s.config.ConfigDir == "" {
		return nil, false
	}

	path, err := resolvePathWithin(s.config.ConfigDir, imageName+".properties")
	if err != nil {
		return nil, false
	}
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, false
	}

	var values map[string]interface{}
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.UseNumber()
	if err := decoder.Decode(&values); err != nil {
		if s.logger != nil {
			s.logger.Warn("Ignoring invalid image properties", "path", path, "error", err)
		}
		return nil, false
	}

	preferred := []string{"vendor", "product", "revision", "block_size"}
	keys := make([]string, 0, len(values))
	seen := make(map[string]struct{}, len(values))
	for _, key := range preferred {
		if _, ok := values[key]; ok {
			keys = append(keys, key)
			seen[key] = struct{}{}
		}
	}
	other := make([]string, 0, len(values)-len(keys))
	for key := range values {
		if _, ok := seen[key]; !ok {
			other = append(other, key)
		}
	}
	sort.Strings(other)
	keys = append(keys, other...)

	metadata := make([]propertyMetadata, 0, len(keys))
	for _, key := range keys {
		value := values[key]
		if value == nil {
			value = ""
		}
		metadata = append(metadata, propertyMetadata{
			Name:  strings.ReplaceAll(key, "_", " "),
			Value: value,
		})
	}
	return metadata, true
}

func imageAndPropertiesPaths(imageRoot, propertiesRoot, imageName string) (string, string, error) {
	imagePath, err := resolvePathWithin(imageRoot, imageName)
	if err != nil {
		return "", "", err
	}
	propertiesPath, err := resolvePathWithin(propertiesRoot, imageName+".properties")
	if err != nil {
		return "", "", err
	}
	return imagePath, propertiesPath, nil
}

func temporarySibling(path, operation string) (string, error) {
	file, err := os.CreateTemp(filepath.Dir(path), "."+filepath.Base(path)+"."+operation+"-*")
	if err != nil {
		return "", err
	}
	tempPath := file.Name()
	if closeErr := file.Close(); closeErr != nil {
		os.Remove(tempPath)
		return "", closeErr
	}
	if err := os.Remove(tempPath); err != nil {
		return "", err
	}
	return tempPath, nil
}

// deleteImageAndProperties stages both files out of their visible locations
// before removing either one. A staging failure is rolled back.
func deleteImageAndProperties(imagePath, propertiesPath string) (bool, error) {
	_, propErr := os.Stat(propertiesPath)
	hasProperties := propErr == nil
	if propErr != nil && !os.IsNotExist(propErr) {
		return false, fmt.Errorf("inspect properties file: %w", propErr)
	}

	stagedImage, err := temporarySibling(imagePath, "delete")
	if err != nil {
		return hasProperties, fmt.Errorf("prepare image deletion: %w", err)
	}
	if err := os.Rename(imagePath, stagedImage); err != nil {
		return hasProperties, fmt.Errorf("stage image deletion: %w", err)
	}

	var stagedProperties string
	if hasProperties {
		stagedProperties, err = temporarySibling(propertiesPath, "delete")
		if err != nil {
			if rollbackErr := os.Rename(stagedImage, imagePath); rollbackErr != nil {
				return true, fmt.Errorf("partial failure preparing properties deletion: %v; image rollback failed: %w", err, rollbackErr)
			}
			return true, fmt.Errorf("prepare properties deletion: %w", err)
		}
		if err := os.Rename(propertiesPath, stagedProperties); err != nil {
			if rollbackErr := os.Rename(stagedImage, imagePath); rollbackErr != nil {
				return true, fmt.Errorf("partial failure staging properties deletion: %v; image rollback failed: %w", err, rollbackErr)
			}
			return true, fmt.Errorf("stage properties deletion: %w", err)
		}
	}

	var cleanupErrors []string
	if err := os.Remove(stagedImage); err != nil {
		cleanupErrors = append(cleanupErrors, fmt.Sprintf("image cleanup: %v", err))
	}
	if hasProperties {
		if err := os.Remove(stagedProperties); err != nil {
			cleanupErrors = append(cleanupErrors, fmt.Sprintf("properties cleanup: %v", err))
		}
	}
	if len(cleanupErrors) > 0 {
		return hasProperties, fmt.Errorf("partial failure after files were staged for deletion: %s", strings.Join(cleanupErrors, "; "))
	}
	return hasProperties, nil
}

func renameImageAndProperties(oldImage, newImage, oldProperties, newProperties string) (bool, error) {
	_, propErr := os.Stat(oldProperties)
	hasProperties := propErr == nil
	if propErr != nil && !os.IsNotExist(propErr) {
		return false, fmt.Errorf("inspect properties file: %w", propErr)
	}
	if _, err := os.Stat(newImage); err == nil {
		return hasProperties, fmt.Errorf("destination image already exists")
	} else if !os.IsNotExist(err) {
		return hasProperties, fmt.Errorf("inspect destination image: %w", err)
	}
	if hasProperties {
		if _, err := os.Stat(newProperties); err == nil {
			return true, fmt.Errorf("destination properties file already exists")
		} else if !os.IsNotExist(err) {
			return true, fmt.Errorf("inspect destination properties file: %w", err)
		}
		if err := os.MkdirAll(filepath.Dir(newProperties), 0o755); err != nil {
			return true, fmt.Errorf("create properties directory: %w", err)
		}
	}

	if err := os.Rename(oldImage, newImage); err != nil {
		return hasProperties, fmt.Errorf("rename image: %w", err)
	}
	if hasProperties {
		if err := os.Rename(oldProperties, newProperties); err != nil {
			if rollbackErr := os.Rename(newImage, oldImage); rollbackErr != nil {
				return true, fmt.Errorf("partial failure renaming properties: %v; image rollback failed: %w", err, rollbackErr)
			}
			return true, fmt.Errorf("rename properties: %w", err)
		}
	}
	return hasProperties, nil
}

func copyFileToTemporary(sourcePath, destinationPath string) (string, error) {
	source, err := os.Open(sourcePath)
	if err != nil {
		return "", err
	}
	defer source.Close()

	info, err := source.Stat()
	if err != nil {
		return "", err
	}
	temp, err := os.CreateTemp(filepath.Dir(destinationPath), "."+filepath.Base(destinationPath)+".copy-*")
	if err != nil {
		return "", err
	}
	tempPath := temp.Name()
	ok := false
	defer func() {
		temp.Close()
		if !ok {
			os.Remove(tempPath)
		}
	}()

	if err := temp.Chmod(info.Mode().Perm()); err != nil {
		return "", err
	}
	if _, err := io.Copy(temp, source); err != nil {
		return "", err
	}
	if err := temp.Sync(); err != nil {
		return "", err
	}
	if err := temp.Close(); err != nil {
		return "", err
	}
	ok = true
	return tempPath, nil
}

func copyImageAndProperties(sourceImage, destinationImage, sourceProperties, destinationProperties string) (bool, error) {
	_, propErr := os.Stat(sourceProperties)
	hasProperties := propErr == nil
	if propErr != nil && !os.IsNotExist(propErr) {
		return false, fmt.Errorf("inspect properties file: %w", propErr)
	}
	if _, err := os.Stat(destinationImage); err == nil {
		return hasProperties, fmt.Errorf("destination image already exists")
	} else if !os.IsNotExist(err) {
		return hasProperties, fmt.Errorf("inspect destination image: %w", err)
	}
	if hasProperties {
		if _, err := os.Stat(destinationProperties); err == nil {
			return true, fmt.Errorf("destination properties file already exists")
		} else if !os.IsNotExist(err) {
			return true, fmt.Errorf("inspect destination properties file: %w", err)
		}
		if err := os.MkdirAll(filepath.Dir(destinationProperties), 0o755); err != nil {
			return true, fmt.Errorf("create properties directory: %w", err)
		}
	}

	tempImage, err := copyFileToTemporary(sourceImage, destinationImage)
	if err != nil {
		return hasProperties, fmt.Errorf("copy image: %w", err)
	}
	defer os.Remove(tempImage)

	var tempProperties string
	if hasProperties {
		tempProperties, err = copyFileToTemporary(sourceProperties, destinationProperties)
		if err != nil {
			return true, fmt.Errorf("copy properties: %w", err)
		}
		defer os.Remove(tempProperties)
		if err := os.Rename(tempProperties, destinationProperties); err != nil {
			return true, fmt.Errorf("install properties copy: %w", err)
		}
	}

	if err := os.Rename(tempImage, destinationImage); err != nil {
		if hasProperties {
			if cleanupErr := os.Remove(destinationProperties); cleanupErr != nil {
				return true, fmt.Errorf("partial failure installing image copy: %v; properties cleanup failed: %w", err, cleanupErr)
			}
		}
		return hasProperties, fmt.Errorf("install image copy: %w", err)
	}
	return hasProperties, nil
}

func writeJSONAtomically(path string, value interface{}) error {
	data, err := json.MarshalIndent(value, "", "    ")
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		return err
	}
	temp, err := os.CreateTemp(filepath.Dir(path), "."+filepath.Base(path)+".write-*")
	if err != nil {
		return err
	}
	tempPath := temp.Name()
	defer os.Remove(tempPath)
	if _, err := temp.Write(data); err != nil {
		temp.Close()
		return err
	}
	if err := temp.Chmod(0o644); err != nil {
		temp.Close()
		return err
	}
	if err := temp.Close(); err != nil {
		return err
	}
	return os.Rename(tempPath, path)
}
