# Installing PiSCSI

The recommended way to install PiSCSI is via the binary deb packages.
Find the latest release on the [GitHub releases page](https://github.com/PiSCSI/piscsi/releases) and download the `piscsi_*.deb` file for your Raspberry Pi architecture.

If you want the Web interface, download the `piscsi-web_*.deb` file as well.
If your Raspberry Pi has an OLED screen, download the `piscsi-oled_*.deb` file, and if you have a PiSCSI Control Board, download the `piscsi-ctrlboard_*.deb` file.

## Runtime dependencies

Use apt to install the packages and their dependencies, for example the mandatory runtime dependencies for the _piscsi_ daemon:

```sh
sudo apt update
sudo apt install libprotobuf-dev libspdlog-dev libpcap-dev
```

The _piscsi_ daemon requires the following packages to be installed on the system:

- `libprotobuf-dev` for the protobuf runtime library
- `libspdlog-dev` for the spdlog runtime library
- `libpcap-dev` for the pcap runtime library

Note that the _-dev_ packages are not strictly required at runtime, but they are the easiest way to install the libraries on Debian-based systems, since
they are named consistently regardless of the Raspberry Pi architecture. The _-dev_ packages are also required to build PiSCSI from source.

In addition, these optional packages enables additional functionality:

- `dhcp-helper` for proxy ARP support
- `parprouted` for proxy ARP support
- `rsyslog` for logging

The _piscsi-web_ package has the following optional runtime dependencies:

- `genisoimage` for creating ISO images
- `unar` for extracting compressed archives
- `dosfstools` for creating FAT filesystems
- `hfdisk` for creating HFS partition tables
- `hfsutils` for creating HFS filesystems
- `groff` for displaying man pages
- `man2html` for converting man pages to HTML
- `disktype` for identifying disk image types

The _piscsi-oled_ and _piscsi-ctrlboard_ packages have the following optional runtime dependency:

- `i2c-tools` for configuring and testing I2C devices

## Installing the packages

Use `dpkg` to install the downloaded packages. For example, if you downloaded the `piscsi_*.deb` and `piscsi-web_*.deb` files to your home directory:

```sh
sudo dpkg -i ~/piscsi_*.deb ~/piscsi-web_*.deb
```

In most cases, dpkg will report missing dependencies. Use `apt` to install them.

## Build from source

This guide builds the C++ PiSCSI daemon and command-line tools from a source checkout.
Note that building the C++ code directly on Raspberry Pi hardware can be very slow.
Cross-compiling on a more powerful host is recommended.

Building from source is most useful for development, testing,
or trying an unreleased change. PiSCSI is developed and tested primarily on
recent Raspberry Pi OS releases.

The C++ code and tests can be built on most Unix-like systems,
but the GPIO hardware interface is only available on a Raspberry Pi with a
PiSCSI hat.

## Build prerequisites

On Raspberry Pi OS, install the PiSCSI C++ build dependencies with:

```sh
sudo apt update
sudo apt install --yes \
  build-essential ca-certificates git protobuf-compiler rsyslog \
  clang libgmock-dev libpcap-dev libprotobuf-dev libspdlog-dev
```

`libgmock-dev` supplies the unit-test dependency. You may omit it when you do
not intend to build or run tests.

For the Meson build system, also install Meson and Ninja:

```sh
sudo apt install --yes meson ninja-build
```

Clone the repository:

```sh
git clone https://github.com/PiSCSI/piscsi.git
cd piscsi
```

The web interface and the optional OLED/control-board programs are built with
Go. Start with the [web interface README](go/piscsi-web/README.md); the OLED
and control-board directories each have their own README as well.

## Build with Meson

Meson is configured from the repository root. A separate build directory keeps
generated files out of the checkout.

```sh
meson setup build
meson compile -C build
```

The default target set builds `piscsi`, `scsictl`, `scsimon`, `scsiloop`,
`scsidump`, and the unit tests. `scsiloop` requires Linux on Raspberry Pi GPIO
hardware.

To select targets explicitly, pass a Meson array option:

```sh
meson setup build -Dtarget=piscsi,scsictl
meson compile -C build
```

Run unit tests when the Google Test/Mock packages are installed:

```sh
meson test -C build
```

Install the Meson-built binaries and installed data with:

```sh
sudo meson install -C build
```

By default this installs executable and manual-page files under `/usr/local`,
along with the other data declared by the Meson project. Use `--prefix` to
change the installation root. You may need to manually adjust the systemd
unit files to match the prefix.

## Build with Make

The legacy Makefile is run from `cpp/`. It compiles C++ sources there and
writes binaries to `cpp/bin/`.

```sh
cd cpp
make -j"$(nproc)" all
```

For a development build with debug symbols and no optimization:

```sh
make -j"$(nproc)" DEBUG=1 all
```

Run the C++ unit tests:

```sh
make -j"$(nproc)" test
```

The Makefile accepts `CROSS_COMPILE`, for example `CROSS_COMPILE=arm-linux-gnueabihf-`, and
`DEFAULT_IMAGE_FOLDER` to change the compiled default image directory.

Install the Make-built development files with:

```sh
sudo make install
```

After a Makefile installation, reload logging and enable/start the service as
needed:

```sh
sudo systemctl restart rsyslog
sudo systemctl enable --now piscsi
sudo systemctl status piscsi
```

The daemon log is written to `/var/log/piscsi.log` when the supplied rsyslog
configuration is active.

## Cleaning and rebuilding

For a clean Makefile build:

```sh
cd cpp
make clean
```

For Meson, remove the build directory and configure it again:

```sh
rm -rf build
meson setup build
```

The two build systems can be used in parallel, but they do not share build artifacts.

For system setup and SCSI hardware guidance, see the
[PiSCSI Setup Instructions](https://github.com/PiSCSI/piscsi/wiki/Setup-Instructions).
