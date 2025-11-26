// Copyright 2025 Eric Helgeson. All rights reserved.
// Copyright 2026 Daniel Markstedt. All rights reserved.
// Use of this source code is governed by the BSD 3-Clause
// license that can be found in the LICENSE file.

package config

import (
	"crypto/hkdf"
	"crypto/sha256"
	"encoding/base64"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"net"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"unicode"

	"golang.org/x/sys/unix"
)

const (
	minimumMasterKeySize  = 32
	maximumSecretFileSize = 16 * 1024
	maximumFileSize       = int64(4 * 1024 * 1024 * 1024 * 1024) // 4 TiB

	sessionAuthenticationInfo = "piscsi-web/session-authentication/v1"
	sessionEncryptionInfo     = "piscsi-web/session-encryption/v1"
)

// These defaults can be overridden at link time with go build -ldflags -X.
var (
	defaultBaseDir        = "/var/lib/piscsi/images"
	defaultSharedDir      = "/var/lib/piscsi/shared"
	defaultConfigDir      = "/var/lib/piscsi/config"
	defaultDataDir        = "/var/lib/piscsi/data"
	defaultSessionKeyFile = "/etc/piscsi-web/session.key"
)

// Config holds application configuration.
type Config struct {
	// Server configuration
	ServerPort int
	ServerHost string

	// PiSCSI daemon configuration
	PiscsiHost  string
	PiscsiPort  int
	PiscsiToken string

	// File paths
	BaseDir      string // Base directory for image files
	SharedDir    string // Shared files directory
	ConfigDir    string // Configuration files directory
	DriverDir    string // Macintosh hard disk driver images
	TemplatesDir string // Templates directory
	StaticDir    string // Static assets directory

	// File size limits
	MaxFileSize int64 // Maximum upload file size in bytes

	// Browser session state (flash messages and appearance preferences)
	SessionCookieHashKey  []byte // HKDF-derived cookie integrity key
	SessionCookieBlockKey []byte // HKDF-derived cookie encryption key
	SessionKeyFile        string // Master-key file, empty for development-only SESSION_KEY
	SessionMaxAge         int    // Session max age in seconds
}

// Load reads and validates all configuration before the server is constructed.
func Load() (*Config, error) {
	serverHost, err := loadHost("SERVER_HOST", "0.0.0.0")
	if err != nil {
		return nil, err
	}
	serverPort, err := loadPort("SERVER_PORT", 8080)
	if err != nil {
		return nil, err
	}
	piscsiHost, err := loadHost("PISCSI_HOST", "localhost")
	if err != nil {
		return nil, err
	}
	piscsiPort, err := loadPort("PISCSI_PORT", 6868)
	if err != nil {
		return nil, err
	}
	maxFileSize, err := loadInt64Range("MAX_FILE_SIZE", 4*1024*1024*1024, 1, maximumFileSize)
	if err != nil {
		return nil, err
	}
	sessionMaxAge, err := loadIntRange("SESSION_MAX_AGE", 86400, 1, int(^uint(0)>>1))
	if err != nil {
		return nil, err
	}
	baseDir, err := loadAbsoluteDirectory("BASE_DIR", defaultBaseDir, true)
	if err != nil {
		return nil, err
	}
	sharedDir, err := loadAbsoluteDirectory("SHARED_DIR", defaultSharedDir, true)
	if err != nil {
		return nil, err
	}
	configDir, err := loadAbsoluteDirectory("CONFIG_DIR", defaultConfigDir, true)
	if err != nil {
		return nil, err
	}
	driverDir, err := loadOptionalAbsoluteDirectory("DRIVER_DIR", filepath.Join(defaultDataDir, "mac-hard-disk-drivers"))
	if err != nil {
		return nil, err
	}
	templatesDir, err := loadDirectory("TEMPLATES_DIR", "web/templates", false)
	if err != nil {
		return nil, err
	}
	staticDir, err := loadDirectory("STATIC_DIR", "web/static", false)
	if err != nil {
		return nil, err
	}
	if err := validateDistinctDataRoots(baseDir, sharedDir, configDir); err != nil {
		return nil, err
	}
	masterKey, sessionKeyFile, err := loadMasterKey()
	if err != nil {
		return nil, err
	}
	if sessionKeyFile != "" {
		for setting, root := range map[string]string{
			"BASE_DIR":   baseDir,
			"SHARED_DIR": sharedDir,
			"CONFIG_DIR": configDir,
		} {
			within, err := pathWithin(root, sessionKeyFile)
			if err != nil {
				return nil, fmt.Errorf("compare SESSION_KEY_FILE with %s: %w", setting, err)
			}
			if within {
				return nil, fmt.Errorf("SESSION_KEY_FILE must not be located within writable %s", setting)
			}
		}
	}
	sessionCookieHashKey, err := deriveKey(masterKey, sessionAuthenticationInfo)
	if err != nil {
		return nil, fmt.Errorf("derive session authentication key: %w", err)
	}
	sessionCookieBlockKey, err := deriveKey(masterKey, sessionEncryptionInfo)
	if err != nil {
		return nil, fmt.Errorf("derive session encryption key: %w", err)
	}
	return &Config{
		ServerPort:            serverPort,
		ServerHost:            serverHost,
		PiscsiHost:            piscsiHost,
		PiscsiPort:            piscsiPort,
		PiscsiToken:           os.Getenv("PISCSI_TOKEN"),
		BaseDir:               baseDir,
		SharedDir:             sharedDir,
		ConfigDir:             configDir,
		DriverDir:             driverDir,
		TemplatesDir:          templatesDir,
		StaticDir:             staticDir,
		MaxFileSize:           maxFileSize,
		SessionCookieHashKey:  sessionCookieHashKey,
		SessionCookieBlockKey: sessionCookieBlockKey,
		SessionKeyFile:        sessionKeyFile,
		SessionMaxAge:         sessionMaxAge,
	}, nil
}

func loadMasterKey() ([]byte, string, error) {
	// A configured file deliberately wins even when SESSION_KEY is also set.
	// An explicitly empty file setting is an error, not a request to fall back.
	if filename, configured := os.LookupEnv("SESSION_KEY_FILE"); configured {
		if strings.TrimSpace(filename) == "" {
			return nil, "", fmt.Errorf("SESSION_KEY_FILE must not be empty")
		}
		key, err := readMasterKeyFile(filename)
		return key, filename, err
	}

	if encoded, configured := os.LookupEnv("SESSION_KEY"); configured {
		key, err := decodeMasterKey("SESSION_KEY", encoded)
		return key, "", err
	}

	key, err := readMasterKeyFile(defaultSessionKeyFile)
	return key, defaultSessionKeyFile, err
}

func readMasterKeyFile(filename string) ([]byte, error) {
	if !filepath.IsAbs(filename) {
		return nil, fmt.Errorf("SESSION_KEY_FILE must be an absolute path")
	}

	pathInfo, err := os.Lstat(filename)
	if err != nil {
		return nil, fmt.Errorf("SESSION_KEY_FILE: open configured file: %w", err)
	}
	if !pathInfo.Mode().IsRegular() {
		return nil, fmt.Errorf("SESSION_KEY_FILE must name a regular file")
	}
	if pathInfo.Mode().Perm()&0o027 != 0 {
		return nil, fmt.Errorf("SESSION_KEY_FILE permissions must not allow group writes or access by others")
	}

	file, err := os.Open(filename)
	if err != nil {
		return nil, fmt.Errorf("SESSION_KEY_FILE: open configured file: %w", err)
	}
	defer file.Close()

	fileInfo, err := file.Stat()
	if err != nil {
		return nil, fmt.Errorf("SESSION_KEY_FILE: inspect configured file: %w", err)
	}
	if !os.SameFile(pathInfo, fileInfo) {
		return nil, fmt.Errorf("SESSION_KEY_FILE changed while it was being opened")
	}
	if !fileInfo.Mode().IsRegular() || fileInfo.Mode().Perm()&0o027 != 0 {
		return nil, fmt.Errorf("SESSION_KEY_FILE permissions or file type changed while it was being opened")
	}
	data, err := io.ReadAll(io.LimitReader(file, maximumSecretFileSize+1))
	if err != nil {
		return nil, fmt.Errorf("SESSION_KEY_FILE: read configured file: %w", err)
	}
	if len(data) > maximumSecretFileSize {
		return nil, fmt.Errorf("SESSION_KEY_FILE exceeds %d bytes", maximumSecretFileSize)
	}

	encoded := strings.TrimSuffix(strings.TrimSuffix(string(data), "\n"), "\r")
	return decodeMasterKey("SESSION_KEY_FILE", encoded)
}

func decodeMasterKey(setting, encoded string) ([]byte, error) {
	if strings.TrimSpace(encoded) == "" {
		return nil, fmt.Errorf("%s must not be empty", setting)
	}
	if strings.IndexFunc(encoded, unicode.IsSpace) >= 0 {
		return nil, fmt.Errorf("%s must contain standard base64 without whitespace", setting)
	}

	key, err := base64.StdEncoding.Strict().DecodeString(encoded)
	if err != nil {
		return nil, fmt.Errorf("%s must contain valid standard base64", setting)
	}
	if len(key) < minimumMasterKeySize {
		return nil, fmt.Errorf("%s must decode to at least %d bytes", setting, minimumMasterKeySize)
	}
	return key, nil
}

func deriveKey(masterKey []byte, info string) ([]byte, error) {
	return hkdf.Key(sha256.New, masterKey, nil, info, 32)
}

func loadPort(setting string, defaultValue int) (int, error) {
	return loadIntRange(setting, defaultValue, 1, 65535)
}

func loadIntRange(setting string, defaultValue, minimum, maximum int) (int, error) {
	raw, configured := os.LookupEnv(setting)
	if !configured {
		return defaultValue, nil
	}
	if strings.TrimSpace(raw) == "" {
		return 0, fmt.Errorf("%s must not be empty", setting)
	}
	value, err := strconv.Atoi(raw)
	if err != nil {
		return 0, fmt.Errorf("%s must be an integer", setting)
	}
	if value < minimum || value > maximum {
		return 0, fmt.Errorf("%s must be between %d and %d", setting, minimum, maximum)
	}
	return value, nil
}

func loadInt64Range(setting string, defaultValue, minimum, maximum int64) (int64, error) {
	raw, configured := os.LookupEnv(setting)
	if !configured {
		return defaultValue, nil
	}
	if strings.TrimSpace(raw) == "" {
		return 0, fmt.Errorf("%s must not be empty", setting)
	}
	value, err := strconv.ParseInt(raw, 10, 64)
	if err != nil {
		return 0, fmt.Errorf("%s must be an integer", setting)
	}
	if value < minimum || value > maximum {
		return 0, fmt.Errorf("%s must be between %d and %d", setting, minimum, maximum)
	}
	return value, nil
}

func loadHost(setting, defaultValue string) (string, error) {
	value, err := loadNonemptyString(setting, defaultValue)
	if err != nil {
		return "", err
	}
	if strings.ContainsAny(value, " \t\r\n/") {
		return "", fmt.Errorf("%s must be a host name or IP address without a port", setting)
	}
	if strings.HasPrefix(value, "[") || strings.HasSuffix(value, "]") {
		return "", fmt.Errorf("%s must not contain IPv6 brackets", setting)
	}
	if net.ParseIP(value) == nil && !validHostname(value) {
		return "", fmt.Errorf("%s must be a valid host name or IP address", setting)
	}
	return value, nil
}

func validHostname(value string) bool {
	if len(value) > 253 || strings.HasPrefix(value, ".") || strings.HasSuffix(value, ".") {
		return false
	}
	for _, label := range strings.Split(value, ".") {
		if label == "" || len(label) > 63 || label[0] == '-' || label[len(label)-1] == '-' {
			return false
		}
		for _, r := range label {
			if (r < 'a' || r > 'z') && (r < 'A' || r > 'Z') &&
				(r < '0' || r > '9') && r != '-' {
				return false
			}
		}
	}
	return true
}

func loadAbsoluteDirectory(setting, defaultValue string, writable bool) (string, error) {
	value, err := loadNonemptyString(setting, defaultValue)
	if err != nil {
		return "", err
	}
	if !filepath.IsAbs(value) {
		return "", fmt.Errorf("%s must be an absolute path", setting)
	}
	return validateDirectory(setting, value, writable)
}

// loadOptionalAbsoluteDirectory permits an absent default directory so optional
// features do not prevent the service from starting. Explicit settings remain
// mandatory and are validated like every other configured directory.
func loadOptionalAbsoluteDirectory(setting, defaultValue string) (string, error) {
	value, err := loadNonemptyString(setting, defaultValue)
	if err != nil {
		return "", err
	}
	if !filepath.IsAbs(value) {
		return "", fmt.Errorf("%s must be an absolute path", setting)
	}
	if _, configured := os.LookupEnv(setting); !configured {
		if _, err := os.Stat(value); errors.Is(err, fs.ErrNotExist) {
			return value, nil
		}
	}
	return validateDirectory(setting, value, false)
}

func loadDirectory(setting, defaultValue string, writable bool) (string, error) {
	value, err := loadNonemptyString(setting, defaultValue)
	if err != nil {
		return "", err
	}
	return validateDirectory(setting, value, writable)
}

func validateDirectory(setting, value string, writable bool) (string, error) {
	info, err := os.Stat(value)
	if err != nil {
		return "", fmt.Errorf("%s: open directory: %w", setting, err)
	}
	if !info.IsDir() {
		return "", fmt.Errorf("%s must name a directory", setting)
	}
	mode := uint32(unix.R_OK | unix.X_OK)
	if writable {
		mode |= unix.W_OK
	}
	if err := unix.Access(value, mode); err != nil {
		access := "readable and searchable"
		if writable {
			access = "readable, writable, and searchable"
		}
		return "", fmt.Errorf("%s must be %s: %w", setting, access, err)
	}
	file, err := os.Open(value)
	if err != nil {
		return "", fmt.Errorf("%s: open directory: %w", setting, err)
	}
	if err := file.Close(); err != nil {
		return "", fmt.Errorf("%s: close directory: %w", setting, err)
	}
	return value, nil
}

func loadNonemptyString(setting, defaultValue string) (string, error) {
	value, configured := os.LookupEnv(setting)
	if !configured {
		return defaultValue, nil
	}
	if strings.TrimSpace(value) == "" {
		return "", fmt.Errorf("%s must not be empty", setting)
	}
	return value, nil
}

func validateDistinctDataRoots(baseDir, sharedDir, configDir string) error {
	roots := []struct {
		setting string
		path    string
	}{
		{setting: "BASE_DIR", path: baseDir},
		{setting: "SHARED_DIR", path: sharedDir},
		{setting: "CONFIG_DIR", path: configDir},
	}
	for i := range roots {
		for j := i + 1; j < len(roots); j++ {
			firstContainsSecond, err := pathWithin(roots[i].path, roots[j].path)
			if err != nil {
				return fmt.Errorf("compare %s with %s: %w", roots[i].setting, roots[j].setting, err)
			}
			secondContainsFirst, err := pathWithin(roots[j].path, roots[i].path)
			if err != nil {
				return fmt.Errorf("compare %s with %s: %w", roots[i].setting, roots[j].setting, err)
			}
			if firstContainsSecond || secondContainsFirst {
				return fmt.Errorf("%s and %s must be separate, non-nested directories", roots[i].setting, roots[j].setting)
			}
		}
	}
	return nil
}

func pathWithin(root, candidate string) (bool, error) {
	canonicalRoot, err := filepath.EvalSymlinks(root)
	if err != nil {
		return false, err
	}
	canonicalCandidate, err := filepath.EvalSymlinks(candidate)
	if err != nil {
		return false, err
	}
	relative, err := filepath.Rel(canonicalRoot, canonicalCandidate)
	if err != nil {
		return false, err
	}
	return relative == "." ||
		(relative != ".." && !strings.HasPrefix(relative, ".."+string(filepath.Separator))), nil
}
