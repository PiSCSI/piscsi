# PiSCSI Control Board client

`piscsi-ctrlboard` is the Go client for the PiSCSI Control Board. It reads the
rotary encoder and buttons through the PCA9554 I2C expander, presents the menu
on a 128x64 SSD1306 I2C display, and sends PiSCSI daemon commands for SCSI-ID
management, image attach/eject, profiles, and system actions. The input path
is kept separate from menu, network, and display work so that hardware events
remain responsive.

It is intended to run on a Raspberry Pi with the Control Board attached. By
default it uses `/dev/i2c-1`, PCA9554 address `0x3f`, SSD1306 address `0x3c`,
and BCM GPIO 9 for the expander interrupt. The PiSCSI daemon is expected at
`localhost:6868`.

The PiSCSI Control Board hardware and original Python client were created by
Benjamin Zeiss, and the Go client is a rewrite of that software.

## I2C and GPIO prerequisites

Enable I2C on Raspberry Pi OS and verify that the board devices are visible:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends --assume-yes i2c-tools raspi-config
sudo raspi-config nonint do_i2c 0
i2cdetect -y 1
```

Reboot if I2C was newly enabled. The usual addresses are `0x3c` for the
display and `0x3f` for the PCA9554. The process also needs access to
`/dev/gpiochip0`; run it with the appropriate device permissions or through
the service manager.

## Build and run

From the shared Go module root, build the native binary and run it on the Pi:

```sh
cd go
go build -o piscsi-ctrlboard ./cmd/piscsi-ctrlboard
./piscsi-ctrlboard
```

The Makefile offers equivalent build targets, including cross-compilation for
Raspberry Pi targets:

```sh
cd go
make build-ctrlboard
make build-ctrlboard-linux-arm64
make build-ctrlboard-linux-armv7
```

Use `--help` to list all options. Common overrides include `--host` and
`--port` for the daemon, `--password` for a daemon token, `--rotation=180` for
an inverted display, and `--transitions` to animate submenu navigation. Set
`--display=false` to disable SSD1306 rendering; the Control Board PCA9554 and
GPIO interrupt are still required.

```sh
./piscsi-ctrlboard --host=piscsi.local --port=6868 --rotation=180
./piscsi-ctrlboard --diagnostic
```

Diagnostic mode logs PCA9554 snapshots, decoded input events, queue drops, and
latency counters. It is useful when verifying encoder direction, button
debounce, I2C addresses, or the GPIO interrupt connection.

## Service

The `piscsi-ctrlboard` Debian package installs
[`piscsi-ctrlboard.service`](../../os_integration/systemd/piscsi-ctrlboard.service).
Enable it after installing the package:

```sh
sudo systemctl enable --now piscsi-ctrlboard
```

Use a systemd drop-in to pass non-default hardware addresses, daemon
connection details, or display options.
