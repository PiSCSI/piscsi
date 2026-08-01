# PiSCSI OLED monitor

`piscsi-oled` is the standalone Go status monitor for 128×32 and 128×64
SSD1306 I2C displays. It shares the PiSCSI daemon client and protobuf bindings
with the web application.

## I2C prerequisites

On Raspberry Pi OS, install the screen package baseline used by
`easyinstall.sh`, then enable I2C:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends --assume-yes \
  i2c-tools raspi-config
sudo raspi-config nonint do_i2c 0
```

Reboot if I2C was newly enabled. The display is normally available as
`/dev/i2c-1`; use `i2cdetect -y 1` to confirm the bus and its address (the
default is `0x3c`).

## Build and run

From the shared Go module root:

```sh
cd go
go build -o piscsi-oled ./cmd/piscsi-oled
./piscsi-oled --rotation=180 --height=64
```

The defaults preserve the previous monitor behavior: a 180-degree, 128×32
display at `/dev/i2c-1`, address `0x3c`, polling `localhost:6868` every 1000
milliseconds. Useful flags are:

- `--rotation=0|180`
- `--height=32|64`
- `--refresh_interval=1000`
- `--horizontal_scroll_step=6` (pixels moved per screen refresh; `0` pauses scrolling)
- `--password=TOKEN`
- `--host=localhost` and `--port=6868`
- `--i2c-device=/dev/i2c-1` and `--i2c-address=0x3c`
- `--screensaver=off|ip|blank`
- `--screensaver-idle-timeout=5m` and `--screensaver-move-interval=30s`

### Screensavers

The optional `ip` screensaver keeps the network-status line visible but moves
it to a different text row periodically. The `blank` screensaver clears the
entire panel. Both activate only after the PiSCSI status has not changed for
the configured idle period; device, authentication, or network-status changes
immediately restore the normal display. Polling continues while a screensaver
is active.

```sh
./piscsi-oled --screensaver=ip --screensaver-idle-timeout=5m \
  --screensaver-move-interval=30s

./piscsi-oled --screensaver=blank --screensaver-idle-timeout=5m
```

The default `--screensaver=off` preserves existing behavior. The duration flags
accept Go durations such as `30s`, `5m`, and `1h`.

Run a physical-display diagnostic without connecting to the PiSCSI daemon:

```sh
./piscsi-oled --height=32 --rotation=180 --diagnostic
```

It shows the embedded startup and shutdown splash screens, clears the panel,
and exits. Test this on both display heights and rotations after installing the
binary; standard CI cannot exercise I2C hardware.

## Service

Install [piscsi-oled.service](../piscsi-oled.service) as `piscsi-oled`. The
unit runs with a dynamic unprivileged user and the `i2c` supplementary group;
ensure the I2C device grants group access. On `SIGINT` or `SIGTERM`, the
monitor shows the shutdown splash, blanks the panel after 700 ms, and exits.
