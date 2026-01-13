# PiSCSI Nix Flake

Cross-compile PiSCSI for Raspberry Pi from any Nix system.

## Build Commands

```bash
# Raspberry Pi 2/3/Zero W (32-bit ARM)
nix build .#piscsi-armv7

# Raspberry Pi 4/5 (64-bit ARM)
nix build .#piscsi-aarch64

# STANDARD board type (instead of FULLSPEC)
nix build .#piscsi-armv7-standard
nix build .#piscsi-aarch64-standard

# Debug builds
nix build .#piscsi-armv7-debug
nix build .#piscsi-aarch64-debug

# Native build (for local testing)
nix build

# Run unit tests
nix build .#test
```

## Development

```bash
nix develop  # Enter dev shell with all build tools
```

## Output

Binaries are in `result/bin/`: `piscsi`, `scsictl`, `scsimon`, `scsiloop`, `scsidump`
