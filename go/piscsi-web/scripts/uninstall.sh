#!/usr/bin/env bash
# Uninstall PiSCSI Go Web Interface

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
INSTALL_DIR="/opt/piscsi/web"
SERVICE_FILE="/etc/systemd/system/piscsi-web.service"

# Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$@${NC}"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    print_msg "$RED" "❌ This script must be run as root (use sudo)"
    exit 1
fi

print_msg "$YELLOW" "🗑️  PiSCSI Go Web Interface Uninstallation"
print_msg "$YELLOW" "==========================================="
echo ""

# Stop service if running
if systemctl is-active --quiet piscsi-web; then
    print_msg "$YELLOW" "Stopping piscsi-web service..."
    systemctl stop piscsi-web
fi
# Disable service
if systemctl is-enabled --quiet piscsi-web 2>/dev/null; then
    print_msg "$YELLOW" "Disabling piscsi-web service..."
    systemctl disable piscsi-web
fi

# Remove service files
if [ -f "$SERVICE_FILE" ]; then
    print_msg "$YELLOW" "Removing systemd service files..."
    rm -f "$SERVICE_FILE"
    systemctl daemon-reload
fi

# Remove installation directory
if [ -d "$INSTALL_DIR" ]; then
    print_msg "$YELLOW" "Removing installation directory: $INSTALL_DIR"
    rm -rf "$INSTALL_DIR"
fi

print_msg "$GREEN" "✅ Uninstallation complete!"
echo ""
print_msg "$YELLOW" "Note: User data directories were preserved:"
echo "  /var/lib/piscsi/images"
echo "  /var/lib/piscsi/shared"
echo "  /var/lib/piscsi/config"
echo "  /var/lib/piscsi-web"
echo "  /etc/piscsi-web (configuration and session master key)"
echo ""
print_msg "$YELLOW" "To remove these directories (WARNING: deletes your data):"
echo "  sudo rm -rf /var/lib/piscsi/images"
echo "  sudo rm -rf /var/lib/piscsi/shared"
echo "  sudo rm -rf /var/lib/piscsi/config"
echo "  sudo rm -rf /var/lib/piscsi-web"
echo "  sudo rm -rf /etc/piscsi-web"
