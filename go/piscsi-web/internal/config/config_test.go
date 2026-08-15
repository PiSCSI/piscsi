package config

import (
	"bytes"
	"encoding/base64"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/gorilla/securecookie"
)

func TestLoadValidConfiguration(t *testing.T) {
	env := configureValidEnvironment(t, bytes.Repeat([]byte{0x42}, minimumMasterKeySize))
	t.Setenv("SERVER_HOST", "127.0.0.1")
	t.Setenv("SERVER_PORT", "18080")
	t.Setenv("PISCSI_HOST", "piscsi.local")
	t.Setenv("PISCSI_PORT", "16868")
	t.Setenv("PISCSI_TOKEN", "daemon-secret")
	t.Setenv("MAX_FILE_SIZE", "1048576")
	t.Setenv("SESSION_MAX_AGE", "3600")
	t.Setenv("DRIVE_PROPERTIES_FILE", "/etc/piscsi-web/drive_properties.json")

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}

	if cfg.ServerHost != "127.0.0.1" || cfg.ServerPort != 18080 {
		t.Errorf("server address = %s:%d", cfg.ServerHost, cfg.ServerPort)
	}
	if cfg.PiscsiHost != "piscsi.local" || cfg.PiscsiPort != 16868 {
		t.Errorf("PiSCSI address = %s:%d", cfg.PiscsiHost, cfg.PiscsiPort)
	}
	if cfg.PiscsiToken != "daemon-secret" {
		t.Errorf("PiscsiToken = %q", cfg.PiscsiToken)
	}
	if cfg.BaseDir != env.baseDir || cfg.SharedDir != env.sharedDir ||
		cfg.ConfigDir != env.configDir || cfg.DriverDir != env.driverDir {
		t.Errorf("configured data directories were not preserved")
	}
	if cfg.TemplatesDir != env.templatesDir || cfg.StaticDir != env.staticDir {
		t.Errorf("configured asset directories were not preserved")
	}
	if cfg.DrivePropertiesFile != "/etc/piscsi-web/drive_properties.json" {
		t.Errorf("DrivePropertiesFile = %q", cfg.DrivePropertiesFile)
	}
	if cfg.MaxFileSize != 1048576 || cfg.SessionMaxAge != 3600 {
		t.Errorf("limits = (%d, %d)", cfg.MaxFileSize, cfg.SessionMaxAge)
	}
	if cfg.SessionKeyFile != env.keyFile {
		t.Errorf("SessionKeyFile = %q, want %q", cfg.SessionKeyFile, env.keyFile)
	}
}

func TestLoadRejectsMalformedConfiguration(t *testing.T) {
	tests := []struct {
		name    string
		setting string
		value   string
	}{
		{name: "empty server host", setting: "SERVER_HOST", value: ""},
		{name: "invalid server host", setting: "SERVER_HOST", value: "bad host"},
		{name: "server host with port", setting: "SERVER_HOST", value: "localhost:8080"},
		{name: "empty server port", setting: "SERVER_PORT", value: ""},
		{name: "malformed server port", setting: "SERVER_PORT", value: "eight"},
		{name: "zero server port", setting: "SERVER_PORT", value: "0"},
		{name: "large server port", setting: "SERVER_PORT", value: "65536"},
		{name: "empty PiSCSI host", setting: "PISCSI_HOST", value: ""},
		{name: "invalid PiSCSI host", setting: "PISCSI_HOST", value: "-invalid"},
		{name: "malformed PiSCSI port", setting: "PISCSI_PORT", value: "6868x"},
		{name: "zero PiSCSI port", setting: "PISCSI_PORT", value: "0"},
		{name: "large PiSCSI port", setting: "PISCSI_PORT", value: "65536"},
		{name: "empty file size", setting: "MAX_FILE_SIZE", value: ""},
		{name: "malformed file size", setting: "MAX_FILE_SIZE", value: "large"},
		{name: "zero file size", setting: "MAX_FILE_SIZE", value: "0"},
		{name: "excessive file size", setting: "MAX_FILE_SIZE", value: "4398046511105"},
		{name: "empty session age", setting: "SESSION_MAX_AGE", value: ""},
		{name: "malformed session age", setting: "SESSION_MAX_AGE", value: "one-day"},
		{name: "zero session age", setting: "SESSION_MAX_AGE", value: "0"},
		{name: "relative base directory", setting: "BASE_DIR", value: "images"},
		{name: "relative shared directory", setting: "SHARED_DIR", value: "shared"},
		{name: "relative config directory", setting: "CONFIG_DIR", value: "config"},
		{name: "relative driver directory", setting: "DRIVER_DIR", value: "drivers"},
		{name: "empty drive properties file", setting: "DRIVE_PROPERTIES_FILE", value: ""},
		{name: "empty templates directory", setting: "TEMPLATES_DIR", value: ""},
		{name: "empty static directory", setting: "STATIC_DIR", value: ""},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			configureValidEnvironment(t, bytes.Repeat([]byte{0x31}, minimumMasterKeySize))
			t.Setenv(tt.setting, tt.value)

			_, err := Load()
			if err == nil {
				t.Fatalf("Load() accepted %s=%q", tt.setting, tt.value)
			}
			if !strings.Contains(err.Error(), tt.setting) {
				t.Fatalf("Load() error = %q, want setting %s", err, tt.setting)
			}
		})
	}
}

func TestLoadRejectsUnusableDirectories(t *testing.T) {
	tests := []string{
		"BASE_DIR",
		"SHARED_DIR",
		"CONFIG_DIR",
		"DRIVER_DIR",
		"TEMPLATES_DIR",
		"STATIC_DIR",
	}
	for _, setting := range tests {
		t.Run(setting+" missing", func(t *testing.T) {
			configureValidEnvironment(t, bytes.Repeat([]byte{0x32}, minimumMasterKeySize))
			t.Setenv(setting, filepath.Join(t.TempDir(), "missing"))
			if _, err := Load(); err == nil || !strings.Contains(err.Error(), setting) {
				t.Fatalf("Load() error = %v", err)
			}
		})
		t.Run(setting+" is a file", func(t *testing.T) {
			configureValidEnvironment(t, bytes.Repeat([]byte{0x33}, minimumMasterKeySize))
			filename := filepath.Join(t.TempDir(), "file")
			if err := os.WriteFile(filename, nil, 0o600); err != nil {
				t.Fatal(err)
			}
			t.Setenv(setting, filename)
			if _, err := Load(); err == nil || !strings.Contains(err.Error(), setting) {
				t.Fatalf("Load() error = %v", err)
			}
		})
	}
}

func TestLoadAllowsMissingDefaultDriverDirectory(t *testing.T) {
	configureValidEnvironment(t, bytes.Repeat([]byte{0x34}, minimumMasterKeySize))
	unsetEnv(t, "DRIVER_DIR")

	previousDataDir := defaultDataDir
	defaultDataDir = t.TempDir()
	t.Cleanup(func() { defaultDataDir = previousDataDir })

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	want := filepath.Join(defaultDataDir, "mac-hard-disk-drivers")
	if cfg.DriverDir != want {
		t.Errorf("DriverDir = %q, want %q", cfg.DriverDir, want)
	}
}

func TestLoadRejectsContradictoryPaths(t *testing.T) {
	t.Run("nested writable roots", func(t *testing.T) {
		env := configureValidEnvironment(t, bytes.Repeat([]byte{0x34}, minimumMasterKeySize))
		t.Setenv("SHARED_DIR", env.baseDir)
		if _, err := Load(); err == nil ||
			!strings.Contains(err.Error(), "BASE_DIR") ||
			!strings.Contains(err.Error(), "SHARED_DIR") {
			t.Fatalf("Load() error = %v", err)
		}
	})

	t.Run("master key in writable root", func(t *testing.T) {
		env := configureValidEnvironment(t, bytes.Repeat([]byte{0x35}, minimumMasterKeySize))
		keyFile := filepath.Join(env.configDir, "session.key")
		if err := os.WriteFile(
			keyFile,
			[]byte(base64.StdEncoding.EncodeToString(bytes.Repeat([]byte{0x35}, 32))+"\n"),
			0o640,
		); err != nil {
			t.Fatal(err)
		}
		t.Setenv("SESSION_KEY_FILE", keyFile)
		if _, err := Load(); err == nil ||
			!strings.Contains(err.Error(), "SESSION_KEY_FILE") ||
			!strings.Contains(err.Error(), "CONFIG_DIR") {
			t.Fatalf("Load() error = %v", err)
		}
	})
}

func TestLoadSessionKeyFileTakesPrecedence(t *testing.T) {
	fileKey := bytes.Repeat([]byte{0x44}, minimumMasterKeySize)
	configureValidEnvironment(t, fileKey)
	t.Setenv("SESSION_KEY", base64.StdEncoding.EncodeToString(bytes.Repeat([]byte{0x55}, minimumMasterKeySize)))

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	want, err := deriveKey(fileKey, sessionAuthenticationInfo)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(cfg.SessionCookieHashKey, want) {
		t.Fatal("SESSION_KEY took precedence over SESSION_KEY_FILE")
	}
	if cfg.SessionKeyFile == "" {
		t.Fatal("SessionKeyFile is empty")
	}
}

func TestLoadAcceptsDevelopmentSessionKey(t *testing.T) {
	configureValidEnvironment(t, bytes.Repeat([]byte{0x61}, minimumMasterKeySize))
	unsetEnv(t, "SESSION_KEY_FILE")
	key := bytes.Repeat([]byte{0x62}, minimumMasterKeySize)
	t.Setenv("SESSION_KEY", base64.StdEncoding.EncodeToString(key))

	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}
	if cfg.SessionKeyFile != "" {
		t.Errorf("SessionKeyFile = %q, want empty", cfg.SessionKeyFile)
	}
	want, _ := deriveKey(key, sessionEncryptionInfo)
	if !bytes.Equal(cfg.SessionCookieBlockKey, want) {
		t.Fatal("development master key was not used")
	}
}

func TestLoadRejectsInvalidMasterKeysWithoutDisclosingThem(t *testing.T) {
	tests := []struct {
		name  string
		value string
	}{
		{name: "empty", value: ""},
		{name: "whitespace", value: " \t "},
		{name: "malformed", value: "not*base64"},
		{name: "embedded whitespace", value: base64.StdEncoding.EncodeToString(bytes.Repeat([]byte{1}, 32))[:20] + " " + base64.StdEncoding.EncodeToString(bytes.Repeat([]byte{1}, 32))[20:]},
		{name: "short", value: base64.StdEncoding.EncodeToString(bytes.Repeat([]byte{2}, 31))},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			configureValidEnvironment(t, bytes.Repeat([]byte{0x63}, minimumMasterKeySize))
			unsetEnv(t, "SESSION_KEY_FILE")
			t.Setenv("SESSION_KEY", tt.value)
			_, err := Load()
			if err == nil {
				t.Fatalf("Load() accepted invalid SESSION_KEY")
			}
			if !strings.Contains(err.Error(), "SESSION_KEY") {
				t.Fatalf("Load() error = %q", err)
			}
			if tt.value != "" && strings.Contains(err.Error(), tt.value) {
				t.Fatal("configuration error disclosed secret content")
			}
		})
	}
}

func TestLoadRejectsInvalidMasterKeyFiles(t *testing.T) {
	tests := []struct {
		name string
		data []byte
		mode os.FileMode
	}{
		{name: "empty", mode: 0o640},
		{name: "malformed", data: []byte("not*base64\n"), mode: 0o640},
		{name: "short", data: []byte(base64.StdEncoding.EncodeToString(bytes.Repeat([]byte{1}, 31)) + "\n"), mode: 0o640},
		{name: "group writable", data: []byte(base64.StdEncoding.EncodeToString(bytes.Repeat([]byte{1}, 32)) + "\n"), mode: 0o660},
		{name: "other readable", data: []byte(base64.StdEncoding.EncodeToString(bytes.Repeat([]byte{1}, 32)) + "\n"), mode: 0o644},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			env := configureValidEnvironment(t, bytes.Repeat([]byte{0x64}, minimumMasterKeySize))
			if err := os.WriteFile(env.keyFile, tt.data, tt.mode); err != nil {
				t.Fatal(err)
			}
			if err := os.Chmod(env.keyFile, tt.mode); err != nil {
				t.Fatal(err)
			}
			if _, err := Load(); err == nil || !strings.Contains(err.Error(), "SESSION_KEY_FILE") {
				t.Fatalf("Load() error = %v", err)
			}
		})
	}

	t.Run("missing", func(t *testing.T) {
		configureValidEnvironment(t, bytes.Repeat([]byte{0x65}, minimumMasterKeySize))
		t.Setenv("SESSION_KEY_FILE", filepath.Join(t.TempDir(), "missing"))
		if _, err := Load(); err == nil || !strings.Contains(err.Error(), "SESSION_KEY_FILE") {
			t.Fatalf("Load() error = %v", err)
		}
	})

	t.Run("symlink", func(t *testing.T) {
		env := configureValidEnvironment(t, bytes.Repeat([]byte{0x66}, minimumMasterKeySize))
		link := filepath.Join(t.TempDir(), "session.key")
		if err := os.Symlink(env.keyFile, link); err != nil {
			t.Fatal(err)
		}
		t.Setenv("SESSION_KEY_FILE", link)
		if _, err := Load(); err == nil || !strings.Contains(err.Error(), "regular file") {
			t.Fatalf("Load() error = %v", err)
		}
	})

	t.Run("explicitly empty file setting wins", func(t *testing.T) {
		configureValidEnvironment(t, bytes.Repeat([]byte{0x67}, minimumMasterKeySize))
		t.Setenv("SESSION_KEY_FILE", "")
		t.Setenv("SESSION_KEY", base64.StdEncoding.EncodeToString(bytes.Repeat([]byte{0x68}, 32)))
		if _, err := Load(); err == nil || !strings.Contains(err.Error(), "SESSION_KEY_FILE") {
			t.Fatalf("Load() error = %v", err)
		}
	})
}

func TestDerivedSessionCookieKeysAreStableAndDomainSeparated(t *testing.T) {
	configureValidEnvironment(t, bytes.Repeat([]byte{0x71}, minimumMasterKeySize))
	first, err := Load()
	if err != nil {
		t.Fatal(err)
	}
	second, err := Load()
	if err != nil {
		t.Fatal(err)
	}

	keys := [][]byte{
		first.SessionCookieHashKey,
		first.SessionCookieBlockKey,
	}
	for i, key := range keys {
		if len(key) != 32 {
			t.Errorf("derived key %d length = %d", i, len(key))
		}
	}
	if !bytes.Equal(first.SessionCookieHashKey, second.SessionCookieHashKey) ||
		!bytes.Equal(first.SessionCookieBlockKey, second.SessionCookieBlockKey) {
		t.Fatal("derived keys changed across restart")
	}
	if bytes.Equal(keys[0], keys[1]) {
		t.Fatal("derived keys are not distinct")
	}
}

func TestDerivedSessionCookieKeysPreserveAndInvalidateCookies(t *testing.T) {
	configureValidEnvironment(t, bytes.Repeat([]byte{0x72}, minimumMasterKeySize))
	first, err := Load()
	if err != nil {
		t.Fatal(err)
	}
	value := map[string]string{"language": "sv"}
	firstCodec := securecookie.New(
		first.SessionCookieHashKey,
		first.SessionCookieBlockKey,
	)
	encoded, err := firstCodec.Encode("piscsi_session", value)
	if err != nil {
		t.Fatal(err)
	}

	restarted, err := Load()
	if err != nil {
		t.Fatal(err)
	}
	var decoded map[string]string
	restartedCodec := securecookie.New(
		restarted.SessionCookieHashKey,
		restarted.SessionCookieBlockKey,
	)
	if err := restartedCodec.Decode("piscsi_session", encoded, &decoded); err != nil {
		t.Fatalf("cookie did not survive restart: %v", err)
	}

	configureValidEnvironment(t, bytes.Repeat([]byte{0x73}, minimumMasterKeySize))
	rotated, err := Load()
	if err != nil {
		t.Fatal(err)
	}
	rotatedCodec := securecookie.New(
		rotated.SessionCookieHashKey,
		rotated.SessionCookieBlockKey,
	)
	if err := rotatedCodec.Decode("piscsi_session", encoded, &decoded); err == nil {
		t.Fatal("cookie survived master-key rotation")
	}
}

type validEnvironment struct {
	baseDir      string
	sharedDir    string
	configDir    string
	driverDir    string
	templatesDir string
	staticDir    string
	keyFile      string
}

func configureValidEnvironment(t *testing.T, masterKey []byte) validEnvironment {
	t.Helper()
	root := t.TempDir()
	env := validEnvironment{
		baseDir:      filepath.Join(root, "images"),
		sharedDir:    filepath.Join(root, "shared"),
		configDir:    filepath.Join(root, "config"),
		driverDir:    filepath.Join(root, "drivers"),
		templatesDir: filepath.Join(root, "templates"),
		staticDir:    filepath.Join(root, "static"),
		keyFile:      filepath.Join(root, "session.key"),
	}
	for _, directory := range []string{
		env.baseDir,
		env.sharedDir,
		env.configDir,
		env.driverDir,
		env.templatesDir,
		env.staticDir,
	} {
		if err := os.MkdirAll(directory, 0o700); err != nil {
			t.Fatal(err)
		}
	}
	encoded := base64.StdEncoding.EncodeToString(masterKey) + "\n"
	if err := os.WriteFile(env.keyFile, []byte(encoded), 0o640); err != nil {
		t.Fatal(err)
	}

	t.Setenv("BASE_DIR", env.baseDir)
	t.Setenv("SHARED_DIR", env.sharedDir)
	t.Setenv("CONFIG_DIR", env.configDir)
	t.Setenv("DRIVER_DIR", env.driverDir)
	t.Setenv("TEMPLATES_DIR", env.templatesDir)
	t.Setenv("STATIC_DIR", env.staticDir)
	t.Setenv("SESSION_KEY_FILE", env.keyFile)
	unsetEnv(t, "SESSION_KEY")
	for _, setting := range []string{
		"SERVER_HOST",
		"SERVER_PORT",
		"PISCSI_HOST",
		"PISCSI_PORT",
		"PISCSI_TOKEN",
		"MAX_FILE_SIZE",
		"SESSION_MAX_AGE",
		"DRIVE_PROPERTIES_FILE",
	} {
		unsetEnv(t, setting)
	}
	return env
}

func unsetEnv(t *testing.T, key string) {
	t.Helper()
	value, configured := os.LookupEnv(key)
	if err := os.Unsetenv(key); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		if configured {
			_ = os.Setenv(key, value)
		} else {
			_ = os.Unsetenv(key)
		}
	})
}
