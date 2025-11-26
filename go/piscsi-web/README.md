# PiSCSI Web Interface (Go)

Go implementation of the PiSCSI web interface, providing a browser-based control panel for the PiSCSI SCSI emulator.

## Components

### Web Interface (`piscsi-web`)

HTTP server providing:

- Immediate access without a login page
- Device management (attach/detach SCSI devices)
- File operations (upload/download/manage disk images)
- System administration
- Server-rendered HTML forms

**Architecture:**

- **Form-post centric**: POST-Redirect-GET pattern with flash messages
- **Minimal JavaScript**: Designed for retro computers (only confirmations/prompts)
- **Template-driven**: Server-side HTML rendering via Go templates

The web interface does not expose a general-purpose JSON API and does not
preserve the former Python web client's `Accept: application/json` response
contract. Form and page routes always use HTML responses or redirects. The
`/healthcheck` endpoint deliberately returns a small JSON response for service
monitoring; downloads and static resources use their corresponding media
types. Any future automation API should be introduced as a separately
documented and versioned interface.

### Mock PiSCSI Daemon (`mock-piscsi`)

Minimal gRPC server simulating the PiSCSI daemon for development/testing without hardware.

## Building

### Baseline Build Environment

For normal PiSCSI installations, use a published PiSCSI binary; Go is needed
only when building from source.

The source build is distribution-independent. It requires:

- Go 1.25 or newer, or Go 1.21 or newer as a bootstrap toolchain
- Git, Make, CA certificates, and OpenSSL
- Network access to download Go modules, unless the module cache is already
  populated

The module requires Go 1.25 and selects a patched Go release with the
`toolchain` directive in `go.mod`. An operating system whose packaged Go
version is older than 1.25 can use Go's automatic toolchain selection instead
of replacing its system compiler. This applies to any distribution with Go
1.21 through 1.24, including Raspberry Pi OS based on Debian Trixie with its
Go 1.24 package.

Go versions older than 1.21 cannot perform automatic toolchain selection.
On such systems, install a current Go toolchain directly, use the release
container, or install a published PiSCSI binary.

The build generates Go bindings from the canonical schema in
`../../proto/piscsi_interface.proto`. It requires the Protocol Buffers compiler
and the Go protobuf generator.

On Debian-family systems, install the bootstrap compiler and native build
dependencies with:

```shell
sudo apt-get update
sudo apt-get install --yes ca-certificates git golang-go make openssl protobuf-compiler
go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.36.11
export PATH="$(go env GOPATH)/bin:$PATH"
```

Install the equivalent packages through the system package manager on other
operating systems.

### Build Commands

**Web interface:**

From `go/piscsi-web`, select the toolchain, verify the source, and build the
application with:

```shell
GOTOOLCHAIN=auto go version
GOTOOLCHAIN=auto go mod download
make test
go vet -tags nomsgpack ./...
make build
```

`make test` is portable to 32-bit ARM and Raspberry Pi kernels that cannot run
Go's race detector. On a supported development host, run `make test-race` to
include race detection.

Run these commands from the module directory so the `go` command sees the
version requirements in `go.mod`. Confirm that the first command reports the
patched toolchain selected by `go.mod`, rather than an older bootstrap
toolchain or an unpatched Go 1.25.0 toolchain.

Automatic toolchain selection requires network access to a Go module proxy and
the checksum database. For a reproducible or offline package build, install the
exact Go toolchain version used by CI in advance, or build in the release
container. Keep the `toolchain` directive in `go.mod` synchronized with
`GO_VERSION` in `.github/workflows/go-web.yml`.

PiSCSI does not expose Gin's optional MessagePack binding or renderer. Supported
builds therefore use Gin's `nomsgpack` build tag, which avoids compiling the
memory-intensive `ugorji/codec` dependency. This is especially important when
building natively on a Raspberry Pi 3.

On a low-memory system, also limit Go to one package build at a time:

```shell
make build GOBUILD='go build -p=1'
```

If native tests also encounter memory pressure, use the same setting:

```shell
make test GOTEST='go test -p=1'
```

Static builds are also available:

```shell
make build-linux-arm64-static
make build-linux-armv7-static
```

**Mock daemon:**

```shell
make mock
```

**Regenerate protobuf bindings:**

```shell
make proto
```

### Custom build-time data-directory defaults

The `BASE_DIR`, `SHARED_DIR`, `CONFIG_DIR`, and `DRIVER_DIR` environment
variables override the application's defaults at runtime.
When building a binary for a legacy installation where data dirs are under `/home/pi`,
the fallback defaults can instead be selected with Makefile variables:

```shell
make build \
  DEFAULT_BASE_DIR=/home/pi/images \
  DEFAULT_SHARED_DIR=/home/pi/shared \
  DEFAULT_CONFIG_DIR=/home/pi/.config/piscsi \
  DEFAULT_DATA_DIR=/home/pi \
  DEFAULT_SESSION_KEY_FILE=/home/pi/.config/piscsi/session.key
```

The resulting binary uses these directories only when the corresponding
environment variable is unset. `DEFAULT_SESSION_KEY_FILE` is used only when
`SESSION_KEY_FILE` is unset. `DEFAULT_DATA_DIR` sets the support-data root;
the default driver directory is its `mac-hard-disk-drivers` subdirectory.

The packaged installation stores root-owned, read-only support data below
`/var/lib/piscsi/data`. The Macintosh driver images occupy its
`mac-hard-disk-drivers` subdirectory, leaving the parent directory available
for future PiSCSI support data.

## Installing

Run the installer from `go/piscsi-web` after building the application. On
x86_64, the native build name can be installed directly:

```shell
make build
sudo make install
```

On Raspberry Pi, name the binary for the target architecture because the
installer uses the architecture suffix to select it:

```shell
# 64-bit Raspberry Pi OS (ARM64)
make build-linux-arm64
sudo make install
```

```shell
# 32-bit Raspberry Pi OS (ARMv7)
make build-linux-armv7
sudo make install
```

The installer copies the selected web binary and assets to `/opt/piscsi/web`,
installs the systemd unit, creates the service account and data directories,
creates an empty default environment file, provisions the browser-session key,
and enables the web service.

### Optional Macintosh driver injection

Macintosh driver injection when creating `Lido 7.56` or `SpeedTools 3.6` HFS
images is optional. The installer does not provide driver images. If you have
the corresponding `Lido-7.56.img` and `SpeedTools-3.6.img` files, install them
manually in the default driver directory:

```shell
sudo install -d -o root -g root -m 0755 /var/lib/piscsi/data/mac-hard-disk-drivers
sudo install -o root -g root -m 0644 /path/to/Lido-7.56.img /path/to/SpeedTools-3.6.img \
  /var/lib/piscsi/data/mac-hard-disk-drivers/
```

Alternatively, set `DRIVER_DIR` to an existing, readable directory containing
those files. Without the driver images, the web service remains available but
the corresponding image-format options cannot be used.

Archive inspection and extraction use the `lsar` and `unar` commands from the
Debian `unar` package. `easyinstall.sh` installs this package automatically.
For a standalone Go installation, install it before starting the service:

```shell
sudo apt-get install --yes unar
```

Verify the installation with:

```shell
sudo systemctl status piscsi-web
sudo journalctl -u piscsi-web -f
```

### Migrating from the Python web interface

The Python web interface stored disk images in `~/images`, shared files in
`~/shared_files`, and saved configurations and image-property files in
`~/.config/piscsi`. After installing the Go web interface, stop the web
service and run:

```shell
sudo systemctl stop piscsi-web
sudo make migrate-data
sudo systemctl start piscsi-web
```

The migration copies these files to the default directories under
`/var/lib/piscsi`, applies the ownership and permissions required by the
`piscsi-web` service, and imports a legacy backend authentication token into
the protected `/etc/piscsi-web/piscsi-web.env` file. Existing destination
files are preserved, and the source files are not removed. It also copies
existing Macintosh driver images from `~/mac-hard-disk-drivers` to
`/var/lib/piscsi/data/mac-hard-disk-drivers`. Use
`./scripts/migrate-data.sh --dry-run` to preview the migration or
`--source-home PATH` when the legacy installation did not use `/home/pi`.

Legacy RaSCSI 21.10 configuration files with a JSON list at the top level
cannot be converted by the migration script. Load and re-save them with the
Python web interface before switching. Current object-format JSON files retain
their schema, with absolute paths under the legacy image directory converted
to portable relative image paths.

## Running

### Development Mode

```shell
# Terminal 1: Start mock daemon
./mock-piscsi

# Terminal 2: Start web interface
SESSION_KEY="$(openssl rand -base64 32)" \
BASE_DIR=/absolute/path/to/images \
SHARED_DIR=/absolute/path/to/shared \
CONFIG_DIR=/absolute/path/to/config \
DRIVER_DIR=/absolute/path/to/mac-hard-disk-drivers \
./piscsi-web
```

Access at: <http://localhost:8080>

Create the configured data directories before starting the process. Direct
`SESSION_KEY` use is development-only and creates a new session identity each
time the example is run. To preserve development sessions across restarts,
write `openssl rand -base64 32` to a mode `0600` file once and configure its
absolute path through `SESSION_KEY_FILE`.

### Saved configurations

The Go web interface uses the current object-format JSON configuration with
`version`, `devices`, and `reserved_ids` fields. Reservation memos are not
supported; a `memo` field in an existing object-format file is ignored.
When `default.json` is present in the configured configuration directory, it
is validated and loaded automatically during web-server startup.

The obsolete RaSCSI 21.10 format, which uses a device list at the top level, is
not supported. To migrate one of these files, load it with the Python web
client and save it again. The resulting object-format file can then be loaded
by the Go web interface.

### Production Mode

On a packaged installation, start the web interface through systemd:

```shell
sudo systemctl start piscsi-web
sudo systemctl status piscsi-web
```

The packaged web service runs as the dedicated `piscsi-web` system user with
the `piscsi` group. The interface is usable immediately after loading and does
not authenticate browser users. Keep it restricted to a trusted network.

Do not run `/opt/piscsi/web/piscsi-web` directly as a regular user: the
production session master key is deliberately readable only by root and the
`piscsi` service group.

For development or a manually launched private instance, use a separate
session key and user-writable data directories rather than changing the
permissions of the production key.

Default configuration:

- Server: `0.0.0.0:8080`
- PiSCSI daemon: `localhost:6868`
- Disk images: `/var/lib/piscsi/images`
- Shared files: `/var/lib/piscsi/shared`
- Saved configuration: `/var/lib/piscsi/config`
- Macintosh hard disk drivers: `/var/lib/piscsi/data/mac-hard-disk-drivers`
- Session master key: `/etc/piscsi-web/session.key`
- Protected configuration: `/etc/piscsi-web/piscsi-web.env`

### System Data Permissions

The packaged services are expected to use the following ownership and
permission modes. The optional support-data rows apply only when Macintosh
driver injection has been configured:

| Path | Owner and group | Mode |
| --- | --- | --- |
| `/var/lib/piscsi` | `root:piscsi` | `2770` |
| `/var/lib/piscsi/images` | `root:piscsi` | `2770` |
| `/var/lib/piscsi/shared` | `root:piscsi` | `2770` |
| `/var/lib/piscsi/config` | `piscsi-web:piscsi` | `2770` |
| `/var/lib/piscsi/data` | `root:root` | `0755` |
| `/var/lib/piscsi/data/mac-hard-disk-drivers` | `root:root` | `0755` |
| `/var/lib/piscsi-web` | `piscsi-web:piscsi` | `0700` |
| `/etc/piscsi-web` | `root:piscsi` | `0750` |
| `/etc/piscsi-web/piscsi-web.env` | `root:piscsi` | `0640` |
| `/etc/piscsi-web/session.key` | `root:piscsi` | `0640` |

Mode `2770` sets the setgid bit on shared directories so newly created files
inherit the `piscsi` group. Both services use `UMask=0007`. The configuration
directory and session key permissions must not be relaxed merely to let regular
users launch the production service manually.

### Session-key provisioning and rotation

The installer preserves a valid `/etc/piscsi-web/session.key` byte-for-byte on
upgrade. It generates a new key only when that file does not exist. A malformed
or short existing key stops the installation instead of replacing it.

The new file contains standard base64 that decodes to at least 32 random bytes.
The application derives separate cookie-authentication and cookie-encryption
keys using HKDF-SHA-256; the master key itself is never logged. The cookie
stores per-browser language and theme preferences plus short-lived flash
messages. Signing prevents client-side tampering, while encryption avoids
exposing operational messages and paths to the browser.

Rotate the production key only as a deliberate maintenance action:

```shell
sudo systemctl stop piscsi-web
umask 077
SESSION_KEY_TMP=$(mktemp)
openssl rand -base64 32 > "$SESSION_KEY_TMP"
sudo install -o root -g piscsi -m 0640 \
  "$SESSION_KEY_TMP" /etc/piscsi-web/session.key
rm -f "$SESSION_KEY_TMP"
sudo systemctl start piscsi-web
```

Rotation immediately invalidates every existing browser preference and pending
flash message. Keep a protected backup of the previous key until the restart is
verified if rollback must preserve these cookies.

## Configuration

Environment variables:

- `PISCSI_HOST`: PiSCSI daemon host (default: `localhost`)
- `PISCSI_PORT`: PiSCSI daemon port (default: `6868`)
- `PISCSI_TOKEN`: Token password for an authenticated PiSCSI daemon; the
  equivalent `--password` command-line option overrides it when non-empty
- `SERVER_HOST`: Web server listen host (default: `0.0.0.0`)
- `SERVER_PORT`: Web server port (default: `8080`)
- `BASE_DIR`: Disk image directory (default: `/var/lib/piscsi/images`)
- `SHARED_DIR`: Shared-file directory (default: `/var/lib/piscsi/shared`)
- `CONFIG_DIR`: Saved configuration directory (default: `/var/lib/piscsi/config`)
- `DRIVER_DIR`: Lido and SpeedTools driver-image directory (default:
  `/var/lib/piscsi/data/mac-hard-disk-drivers`)
- `TEMPLATES_DIR`: HTML template directory (default: `web/templates`)
- `STATIC_DIR`: Static-asset directory (default: `web/static`)
- `MAX_FILE_SIZE`: Maximum upload size in bytes (default: `4294967296`)
- `SESSION_MAX_AGE`: Session lifetime in seconds (default: `86400`)
- `SESSION_KEY_FILE`: Protected base64 master-key file (default:
  `/etc/piscsi-web/session.key`)
- `SESSION_KEY`: Development-only base64 master key used only when
  `SESSION_KEY_FILE` is not configured

`SESSION_KEY_FILE` takes precedence when both secret settings are present,
including when the file setting is explicitly empty or invalid. Production
deployments must use the file. All configured numbers, hosts, paths, and
required directory access are validated before the server is constructed;
malformed values never fall back to defaults.

Startup fails if the session key is missing, malformed, too short, group-writable, or
accessible by other users, if a required directory is unusable, or if the
PiSCSI daemon requires a missing or invalid token.

Because there is no browser authentication, public-internet exposure is
unsupported. Keep the service on a trusted LAN.

## Updating Localizations

The gettext catalogs are stored as uncompiled PO files under
`web/translations/<locale>/LC_MESSAGES/messages.po` and are embedded directly
in the binary. Install GNU gettext (`gettext` on Debian and Homebrew) before
maintaining them.

The Go renderer looks up the final English text produced by the templates and
handlers. From `go/piscsi-web`, extract these messages, regenerate
`messages.pot`, merge the template into every PO file, and validate the
results with:

```shell
./scripts/update-translations.sh
```

The script stages and validates the generated POT and every merged PO file
before replacing any source file. Existing translations are retained when
their message IDs still match. Review new, fuzzy, and obsolete entries after
running it, update their translations, and run the script again before
committing `messages.pot` and the PO files.

## Project Structure

```shell
.
├── cmd/
│   ├── piscsi-web/       # Web server entrypoint
│   └── mock-piscsi/      # Mock daemon entrypoint
├── internal/
│   ├── server/         # HTTP handlers, routing, flash messages
│   ├── piscsi/         # gRPC client for PiSCSI daemon
│   ├── config/         # Configuration management
│   └── driveprops/     # Drive properties database
├── web/
│   ├── templates/      # HTML templates
│   └── static/         # CSS, JS, images
├── drive_properties.json
└── mock-piscsi         # Built mock daemon
```

## Development Notes

- Templates are embedded in the binary via `embed` directives
- Session storage uses gorilla/sessions with cookie-based persistence
- Flash messages require `gob.Register()` for serialization
- `../../proto/piscsi_interface.proto` is the canonical protocol definition.
- Go bindings are generated automatically by Make targets and are not committed.
