#!/usr/bin/env bash
# Install PiSCSI Go Web Interface on Raspberry Pi

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
INSTALL_DIR="/opt/piscsi/web"
SERVICE_FILE="piscsi-web.service"
BINARY_NAME="piscsi-web"
SERVICE_USER="piscsi-web"
SERVICE_GROUP="piscsi"
PISCSI_DATA_DIR="/var/lib/piscsi"
BASE_DIR="$PISCSI_DATA_DIR/images"
SHARED_DIR="$PISCSI_DATA_DIR/shared"
CONFIG_DIR="$PISCSI_DATA_DIR/config"
WEB_STATE_DIR="/var/lib/piscsi-web"
WEB_CONFIG_DIR="/etc/piscsi-web"
WEB_ENV_FILE="$WEB_CONFIG_DIR/piscsi-web.env"
SESSION_KEY_FILE="$WEB_CONFIG_DIR/session.key"

# Detect architecture
detect_arch() {
    local arch=$(uname -m)
    case "$arch" in
        aarch64|arm64)
            echo "arm64"
            ;;
        armv7l|armhf)
            echo "armv7"
            ;;
        x86_64|amd64)
            echo "x86_64"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

# Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$@${NC}"
}

provision_session_key() {
    local key_file=$1
    local key_owner=$2
    local key_group=$3
    local decoded_size
    local key_tmp

    if [ -L "$key_file" ]; then
        print_msg "$RED" "❌ Refusing symbolic-link session key: $key_file"
        return 1
    fi

    if [ -e "$key_file" ]; then
        if [ ! -f "$key_file" ]; then
            print_msg "$RED" "❌ Session key is not a regular file: $key_file"
            return 1
        fi

        # The application accepts one line of standard base64 which decodes to
        # at least 32 bytes. Validate before preserving an existing key.
        if ! awk 'NR != 1 || $0 !~ "^[A-Za-z0-9+/]+={0,2}$" { exit 1 } END { if (NR != 1) exit 1 }' "$key_file"; then
            print_msg "$RED" "❌ Existing session key is not one line of standard base64"
            return 1
        fi
        if ! decoded_size=$(base64 --decode "$key_file" 2>/dev/null | wc -c); then
            print_msg "$RED" "❌ Existing session key is not valid standard base64"
            return 1
        fi
        if [ "$decoded_size" -lt 32 ]; then
            print_msg "$RED" "❌ Existing session key must decode to at least 32 bytes"
            return 1
        fi

        chown "$key_owner:$key_group" "$key_file"
        chmod 0640 "$key_file"
        return
    fi

    key_tmp=$(mktemp "$WEB_CONFIG_DIR/.session.key.XXXXXX")
    if ! openssl rand -base64 32 > "$key_tmp"; then
        rm -f "$key_tmp"
        print_msg "$RED" "❌ Failed to generate session key"
        return 1
    fi
    install -o "$key_owner" -g "$key_group" -m 0640 "$key_tmp" "$key_file"
    rm -f "$key_tmp"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    print_msg "$RED" "❌ This script must be run as root (use sudo)"
    exit 1
fi

print_msg "$GREEN" "🚀 PiSCSI Go Web Interface Installation"
print_msg "$GREEN" "========================================"
echo ""

# Detect architecture
ARCH=$(detect_arch)
if [ "$ARCH" = "unknown" ]; then
    print_msg "$RED" "❌ Unsupported architecture: $(uname -m)"
    exit 1
fi

print_msg "$YELLOW" "Detected architecture: $ARCH"

# Determine binary filename
if [ "$ARCH" = "arm64" ]; then
    BINARY_SOURCE="$BINARY_NAME-arm64"
elif [ "$ARCH" = "armv7" ]; then
    BINARY_SOURCE="$BINARY_NAME-armv7"
else
    BINARY_SOURCE="$BINARY_NAME"
fi

# Check if binaries exist
if [ ! -f "$BINARY_SOURCE" ]; then
    print_msg "$RED" "❌ Binary not found: $BINARY_SOURCE"
    print_msg "$YELLOW" "Please build the binary first with: make build-linux-$ARCH"
    exit 1
fi
# Stop existing services if running
if systemctl is-active --quiet piscsi-web; then
    print_msg "$YELLOW" "Stopping existing piscsi-web service..."
    systemctl stop piscsi-web
fi
# Create installation directory
print_msg "$YELLOW" "Creating installation directory: $INSTALL_DIR"
mkdir -p "$INSTALL_DIR"

# Copy binary
print_msg "$YELLOW" "Installing binary..."
cp "$BINARY_SOURCE" "$INSTALL_DIR/$BINARY_NAME"
chmod +x "$INSTALL_DIR/$BINARY_NAME"

# Copy web assets
if [ -d "web" ]; then
    print_msg "$YELLOW" "Installing web assets..."
    cp -r web "$INSTALL_DIR/"
fi
HFS_MAP_SOURCE="../../python/web/genisoimage_hfs_resource_fork_map.txt"
if [ -f "$HFS_MAP_SOURCE" ]; then
    install -m 0644 "$HFS_MAP_SOURCE" "$INSTALL_DIR/web/genisoimage_hfs_resource_fork_map.txt"
fi
if [ -f "drive_properties.json" ]; then
    cp drive_properties.json "$INSTALL_DIR/"
fi

# Create service identity
if ! getent group "$SERVICE_GROUP" >/dev/null; then
    groupadd --system "$SERVICE_GROUP"
fi
if ! getent passwd "$SERVICE_USER" >/dev/null; then
    useradd --system --gid "$SERVICE_GROUP" --home-dir /nonexistent \
        --shell /usr/sbin/nologin "$SERVICE_USER"
fi

# Install systemd service
print_msg "$YELLOW" "Installing systemd service..."
cp "$SERVICE_FILE" /etc/systemd/system/

# Create required directories
print_msg "$YELLOW" "Creating required directories..."
install -d -o root -g "$SERVICE_GROUP" -m 2770 "$PISCSI_DATA_DIR"
install -d -o root -g "$SERVICE_GROUP" -m 2770 "$BASE_DIR"
install -d -o root -g "$SERVICE_GROUP" -m 2770 "$SHARED_DIR"
install -d -o "$SERVICE_USER" -g "$SERVICE_GROUP" -m 2770 "$CONFIG_DIR"
install -d -o "$SERVICE_USER" -g "$SERVICE_GROUP" -m 0700 "$WEB_STATE_DIR"

install -d -o root -g "$SERVICE_GROUP" -m 0750 "$WEB_CONFIG_DIR"
if [ -L "$WEB_ENV_FILE" ]; then
    print_msg "$RED" "❌ Refusing symbolic-link configuration file: $WEB_ENV_FILE"
    exit 1
elif [ ! -e "$WEB_ENV_FILE" ]; then
    print_msg "$YELLOW" "Installing default web configuration..."
    install -o root -g "$SERVICE_GROUP" -m 0640 /dev/null "$WEB_ENV_FILE"
else
    print_msg "$YELLOW" "Preserving existing web configuration..."
    chown root:"$SERVICE_GROUP" "$WEB_ENV_FILE"
    chmod 0640 "$WEB_ENV_FILE"
fi

print_msg "$YELLOW" "Provisioning session master key..."
provision_session_key \
    "$SESSION_KEY_FILE" \
    root \
    "$SERVICE_GROUP"

# Set ownership
print_msg "$YELLOW" "Setting ownership..."
chown -R root:root "$INSTALL_DIR"
chmod -R go-w "$INSTALL_DIR"
chown root:"$SERVICE_GROUP" "$SESSION_KEY_FILE"
chmod 0640 "$SESSION_KEY_FILE"

# Reload systemd
print_msg "$YELLOW" "Reloading systemd..."
systemctl daemon-reload

# Enable services
print_msg "$YELLOW" "Enabling PiSCSI Web service..."
systemctl enable piscsi-web

# Start services
print_msg "$YELLOW" "Starting PiSCSI Web service..."
systemctl start piscsi-web

# Check status
sleep 2
if systemctl is-active --quiet piscsi-web; then
    print_msg "$GREEN" "✅ Installation complete!"
    echo ""
    print_msg "$GREEN" "Service Status:"
    systemctl status piscsi-web --no-pager -l
    echo ""
    print_msg "$GREEN" "Web interface should be available at: http://$(hostname -I | awk '{print $1}'):8080"
    echo ""
    print_msg "$YELLOW" "Useful commands:"
    echo "  sudo systemctl status piscsi-web   # Check service status"
    echo "  sudo systemctl restart piscsi-web  # Restart service"
    echo "  sudo systemctl stop piscsi-web     # Stop service"
    echo "  sudo journalctl -u piscsi-web -f   # View logs"
else
    print_msg "$RED" "❌ Service failed to start"
    print_msg "$YELLOW" "Check logs with: sudo journalctl -u piscsi-web -n 50"
    exit 1
fi
