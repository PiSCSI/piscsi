// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package server

import (
	"fmt"
	"os"
	"path/filepath"

	"github.com/gin-gonic/gin"
	"github.com/piscsi/piscsi/go/piscsi/configuration"
	pb "github.com/piscsi/piscsi/go/proto"
)

const (
	configFileSuffix      = configuration.FileSuffix
	defaultConfigFilename = "default.json"
)

// Keep these narrow wrappers while server handlers and their tests migrate to
// the shared package. The configuration format and validation now have one
// implementation for the web UI and Control Board.
type savedConfiguration = configuration.Configuration

func normalizeConfigFilename(name string) (string, error) {
	return configuration.NormalizeFilename(name)
}

func marshalConfiguration(info *pb.PbServerInfo) ([]byte, error) { return configuration.Marshal(info) }

func parseConfiguration(data []byte) (*savedConfiguration, []*pb.PbDeviceDefinition, []int32, error) {
	return configuration.Parse(data)
}

func (s *Server) loadConfigurationFile(c *gin.Context, filename string) error {
	loader := configuration.Loader{
		ConfigDir: s.config.ConfigDir,
		ImageDir:  s.config.BaseDir,
		Client:    s.piscsiClient,
		Commands:  s.getCommandBuilder(c),
	}
	return loader.Load(filename)
}

// loadDefaultConfiguration loads default.json when it exists. Missing default
// configuration is normal and does not issue any daemon commands.
func (s *Server) loadDefaultConfiguration() (bool, error) {
	path := filepath.Join(s.config.ConfigDir, defaultConfigFilename)
	if _, err := os.Stat(path); err != nil {
		if os.IsNotExist(err) {
			return false, nil
		}
		return false, fmt.Errorf("inspect default configuration: %w", err)
	}
	if err := s.loadConfigurationFile(nil, defaultConfigFilename); err != nil {
		return false, err
	}
	return true, nil
}
