#!/usr/bin/env bash
# Build PiSCSI web interface for all platforms

set -euo pipefail

echo "🚀 Building PiSCSI Web Interface for all platforms"
echo "=================================================="
echo ""

# Navigate to project root
cd "$(dirname "$0")/.."

# Clean previous builds
echo "🧹 Cleaning previous builds..."
make clean
echo ""

# Build for native platform (development)
echo "🔨 Building for native platform (x86_64)..."
make build
echo ""

# Build for Raspberry Pi ARM64 (Pi 4/5)
echo "🔨 Building for ARM64 (Raspberry Pi 4/5)..."
make build-linux-arm64
echo ""

# Build for Raspberry Pi ARMv7 (Pi 2/3)
echo "🔨 Building for ARMv7 (Raspberry Pi 2/3)..."
make build-linux-armv7
echo ""

# Display results
echo "✅ Build complete!"
echo ""
echo "Binaries created:"
echo "================="
artifacts=(
    piscsi-web
    piscsi-web-arm64
    piscsi-web-armv7
)
ls -lh "${artifacts[@]}" | awk '{printf "  %-25s %8s\n", $9, $5}'
echo ""

# Calculate checksums
echo "📝 Generating checksums..."
sha256sum "${artifacts[@]}" > checksums.txt
echo "  Checksums saved to: checksums.txt"
echo ""

echo "🎉 All builds completed successfully!"
echo ""
echo "Next steps:"
echo "  1. Test binaries on target hardware"
echo "  2. Deploy to Raspberry Pi"
echo "  3. Configure systemd service"
