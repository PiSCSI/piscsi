// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package main

import (
	"flag"
	"log"
	"log/slog"
	"os"

	"github.com/piscsi/piscsi/go/piscsi-web/internal/config"
	"github.com/piscsi/piscsi/go/piscsi-web/internal/server"
)

func main() {
	password := flag.String("password", "", "token password for authenticating with the PiSCSI daemon")
	flag.Parse()

	// Set up structured logging
	logger := slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	}))
	slog.SetDefault(logger)

	// Load configuration
	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("Failed to load configuration: %v", err)
	}
	if *password != "" {
		cfg.PiscsiToken = *password
	}

	logger.Info("Starting PiSCSI Web Interface",
		"version", "1.0.0-alpha",
		"port", cfg.ServerPort,
		"piscsi_host", cfg.PiscsiHost,
		"piscsi_port", cfg.PiscsiPort,
	)

	// Create and start server
	srv, err := server.New(cfg, logger)
	if err != nil {
		log.Fatalf("Failed to initialize server: %v", err)
	}

	// Start server
	if err := srv.Start(); err != nil {
		log.Fatalf("Failed to start server: %v", err)
	}
}
