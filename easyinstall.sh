#!/usr/bin/env bash

# BSD 3-Clause License
# Author @sonique6784
# Copyright (c) 2020, sonique6784

function showPiSCSILogo(){
logo="""
    .~~.   .~~.\n
  '. \ ' ' / .'\n
   .╔═══════╗.\n
  : ║|¯¯¯¯¯|║ :\n
 ~ (║|_____|║) ~\n
( : ║ .  __ ║ : )\n
 ~ .╚╦═════╦╝. ~\n
  (  ¯¯¯¯¯¯¯  ) PiSCSI Assistant\n
   '~ .~~~. ~'\n
       '~'\n
"""
echo -e $logo
}

function showMacNetworkWired(){
logo="""
                              .-~-.-~~~-.~-.\n
 ╔═══════╗                  .(              )\n
 ║|¯¯¯¯¯|║                 /               \`.\n
 ║|_____|║>--------------<~               .   )\n
 ║ .  __ ║                 (              :'-'\n
 ╚╦═════╦╝                  ~-.________.:'\n
  ¯¯¯¯¯¯¯\n
"""
echo -e $logo
}

function showMacNetworkWireless(){
logo="""
                              .-~-.-~~~-.~-.\n
 ╔═══════╗        .(       .(              )\n
 ║|¯¯¯¯¯|║  .(  .(        /               \`.\n
 ║|_____|║ .o    o       ~               .   )\n
 ║ .  __ ║  '(  '(        (              :'-'\n
 ╚╦═════╦╝        '(       ~-.________.:'\n
  ¯¯¯¯¯¯¯\n
"""
echo -e $logo
}

CONNECT_TYPE="FULLSPEC"
COMPILER="clang++"
MEM=$(grep MemTotal /proc/meminfo | awk '{print $2}')
CORES=`expr $MEM / 450000`
if [ $CORES -gt $(nproc) ]; then
        CORES=$(nproc)
elif [ $CORES -lt 1 ]; then
        CORES=1
fi
USER=$(whoami)
BASE=$(dirname "$(readlink -f "${0}")")
CPP_PATH="$BASE/cpp"
VIRTUAL_DRIVER_PATH="$HOME/images"
CFG_PATH="$HOME/.config/piscsi"
WEB_INSTALL_PATH="$BASE/python/web"
OLED_INSTALL_PATH="$BASE/python/oled"
CTRLBOARD_INSTALL_PATH="$BASE/python/ctrlboard"
PYTHON_COMMON_PATH="$BASE/python/common"
SYSTEMD_PATH="/etc/systemd/system"
SSL_CERTS_PATH="/etc/ssl/certs"
SSL_KEYS_PATH="/etc/ssl/private"
HFDISK_BIN=/usr/bin/hfdisk
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
GIT_REMOTE=${GIT_REMOTE:-origin}
TOKEN=""
AUTH_GROUP="piscsi"
SECRET_FILE="$HOME/.config/piscsi/secret"
FILE_SHARE_PATH="$HOME/shared_files"
FILE_SHARE_NAME="Pi File Server"

APT_PACKAGES_COMMON="build-essential git protobuf-compiler bridge-utils ca-certificates"
APT_PACKAGES_BACKEND="libspdlog-dev libpcap-dev libprotobuf-dev protobuf-compiler libgmock-dev clang"
APT_PACKAGES_PYTHON="python3 python3-dev python3-pip python3-venv python3-setuptools python3-wheel libev-dev libevdev2"
APT_PACKAGES_WEB="nginx-light genisoimage man2html hfsutils dosfstools kpartx unzip unar disktype gettext"

set -e

# checks to run before entering the script main menu
function initialChecks() {
    if [ "root" == "$USER" ]; then
        echo "Do not run this script as $USER or with 'sudo'."
        exit 1
    fi
}

# Only to be used for pi-gen automated install
function cacheSudo() {
    echo "Caching sudo password"
    echo raspberry | sudo -v -S
}

# checks that the current user has sudoers privileges
function sudoCheck() {
    if [[ $HEADLESS ]]; then
        echo "Skipping password check in headless mode"
        return 0
    fi
    echo "Input your password to allow this script to make the above changes."
    sudo -v
}

# Delete file if it exists
function deleteFile() {
    if sudo test -f "$1/$2"; then
        sudo rm "$1/$2" || exit 1
        echo "Deleted file $1/$2"
    fi
}

# Delete dir if it exists
function deleteDir() {
    if sudo test -d "$1"; then
        sudo rm -rf "$1" || exit 1
        echo "Deleted directory $1"
    fi
}

# install all dependency packages for PiSCSI Service
function installPackages() {
    if [[ $SKIP_PACKAGES ]]; then
        echo "Skipping package installation"
        return 0
    fi
    sudo apt-get update && sudo DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends --assume-yes -qq \
        $APT_PACKAGES_COMMON \
        $APT_PACKAGES_BACKEND \
        $APT_PACKAGES_PYTHON \
        $APT_PACKAGES_WEB
}

# install Debian packages for PiSCSI standalone
function installPackagesStandalone() {
    if [[ $SKIP_PACKAGES ]]; then
        echo "Skipping package installation"
        return 0
    fi
    sudo apt-get update && sudo DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends --assume-yes -qq \
        $APT_PACKAGES_COMMON \
        $APT_PACKAGES_BACKEND
}

# install Debian packages for PiSCSI web UI standalone
function installPackagesWeb() {
    if [[ $SKIP_PACKAGES ]]; then
        echo "Skipping package installation"
        return 0
    fi
    sudo apt-get update && sudo DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends --assume-yes -qq \
        $APT_PACKAGES_COMMON \
        $APT_PACKAGES_PYTHON \
        $APT_PACKAGES_WEB
}

# cache the pip packages
function cachePipPackages(){
    pushd $WEB_INSTALL_PATH
    sudo pip3 install -r ./requirements.txt
    popd
}

# compile the PiSCSI binaries
function compilePiscsi() {
    cd "$CPP_PATH" || exit 1

    echo "Compiling $CONNECT_TYPE with $COMPILER on $CORES simultaneous cores..."
    if [[ $SKIP_MAKE_CLEAN ]]; then
        echo "Skipping 'make clean'"
    else
        make clean </dev/null
    fi

    make CXX="$COMPILER" CONNECT_TYPE="$CONNECT_TYPE" -j "$CORES" all </dev/null
}

# install the PiSCSI binaries and modify the service configuration
function installPiscsi() {
    OUTDATED_MAN_PAGE_DIR="/usr/share/man/man1"
    CURRENT_MAN_PAGE_DIR="/usr/local/man/man1"
    CURRENT_BIN_DIR="/usr/local/bin"
    # clean up outdated binaries and man pages if they exist
    deleteFile "$OUTDATED_MAN_PAGE_DIR" "rascsi.1"
    deleteFile "$OUTDATED_MAN_PAGE_DIR" "rasctl.1"
    deleteFile "$OUTDATED_MAN_PAGE_DIR" "scsimon.1"
    deleteFile "$OUTDATED_MAN_PAGE_DIR" "rasdump.1"
    deleteFile "$OUTDATED_MAN_PAGE_DIR" "sasidump.1"
    deleteFile "$CURRENT_MAN_PAGE_DIR" "rascsi.1"
    deleteFile "$CURRENT_MAN_PAGE_DIR" "rasctl.1"
    deleteFile "$CURRENT_MAN_PAGE_DIR" "rasdump.1"
    deleteFile "$CURRENT_MAN_PAGE_DIR" "sasidump.1"
    deleteFile "$CURRENT_BIN_DIR" "rascsi"
    deleteFile "$CURRENT_BIN_DIR" "rasctl"
    deleteFile "$CURRENT_BIN_DIR" "rasdump"
    deleteFile "$CURRENT_BIN_DIR" "sasidump"

    # install
    sudo make install CONNECT_TYPE="$CONNECT_TYPE" </dev/null
}

# Update the systemd configuration for piscsi
function configurePiscsiService() {
    if [[ -f $SECRET_FILE ]]; then
        sudo sed -i "\@^ExecStart.*@ s@@& -F $VIRTUAL_DRIVER_PATH -P $SECRET_FILE@" "$SYSTEMD_PATH/piscsi.service"
        echo "Secret token file $SECRET_FILE detected. Using it to enable back-end authentication."
    else
        sudo sed -i "\@^ExecStart.*@ s@@& -F $VIRTUAL_DRIVER_PATH@" "$SYSTEMD_PATH/piscsi.service"
    fi
    echo "Configured piscsi.service to use $VIRTUAL_DRIVER_PATH as default image dir."
}

# Prepare shared Python code
function preparePythonCommon() {
    PISCSI_PYTHON_PROTO="piscsi_interface_pb2.py"
    deleteFile "$WEB_INSTALL_PATH/src" "rascsi_interface_pb2.py"
    deleteFile "$OLED_INSTALL_PATH/src" "rascsi_interface_pb2.py"
    deleteFile "$PYTHON_COMMON_PATH/src" "rascsi_interface_pb2.py"
    deleteFile "$WEB_INSTALL_PATH/src" "$PISCSI_PYTHON_PROTO"
    deleteFile "$OLED_INSTALL_PATH/src" "$PISCSI_PYTHON_PROTO"
    deleteFile "$PYTHON_COMMON_PATH/src" "$PISCSI_PYTHON_PROTO"

    echo "Compiling the Python protobuf library $PISCSI_PYTHON_PROTO..."
    protoc -I="$CPP_PATH" --python_out="$PYTHON_COMMON_PATH/src" piscsi_interface.proto
}

# install everything required to run an HTTP server (Nginx + Python Flask App)
function installPiscsiWebInterface() {
    sudo cp -f "$WEB_INSTALL_PATH/service-infra/nginx-default.conf" /etc/nginx/sites-available/default
    sudo cp -f "$WEB_INSTALL_PATH/service-infra/502.html" /var/www/html/502.html

    deleteFile "$SSL_CERTS_PATH" "rascsi-web.crt"
    deleteFile "$SSL_KEYS_PATH" "rascsi-web.key"

    # Deleting previous venv dir, if one exists, to avoid the common issue of broken python dependencies
    deleteDir "$WEB_INSTALL_PATH/venv"

    if [ -f "$SSL_CERTS_PATH/piscsi-web.crt" ]; then
        echo "SSL certificate $SSL_CERTS_PATH/piscsi-web.crt already exists."
    else
        echo "SSL certificate $SSL_CERTS_PATH/piscsi-web.crt does not exist; creating self-signed certificate..."
        sudo mkdir -p "$SSL_CERTS_PATH" || true
        sudo mkdir -p "$SSL_KEYS_PATH" || true
        sudo openssl req -x509 -nodes -sha256 -days 3650 \
            -newkey rsa:4096 \
            -keyout "$SSL_KEYS_PATH/piscsi-web.key" \
            -out "$SSL_CERTS_PATH/piscsi-web.crt" \
            -subj '/CN=piscsi' \
            -addext 'subjectAltName=DNS:piscsi' \
            -addext 'extendedKeyUsage=serverAuth'
    fi

    sudo systemctl reload nginx || true
}

# Creates the dir that PiSCSI uses to store image files
function createImagesDir() {
    if [ -d "$VIRTUAL_DRIVER_PATH" ]; then
        echo "The $VIRTUAL_DRIVER_PATH directory already exists."
    else
        echo "The $VIRTUAL_DRIVER_PATH directory does not exist; creating..."
        mkdir -p "$VIRTUAL_DRIVER_PATH"
        chmod -R 775 "$VIRTUAL_DRIVER_PATH"
    fi
}

# Creates the dir that the Web Interface uses to store configuration files
function createCfgDir() {
    if [ -d "$HOME/.config/rascsi" ]; then
        sudo mv "$HOME/.config/rascsi" "$CFG_PATH"
        echo "Renamed the rascsi config dir as $CFG_PATH."
    elif [ -d "$CFG_PATH" ]; then
        echo "The $CFG_PATH directory already exists."
    else
        echo "The $CFG_PATH directory does not exist; creating..."
        mkdir -p "$CFG_PATH"
        chmod -R 775 "$CFG_PATH"
    fi
}

# Checks for upstream changes to the git repo and fast-forwards changes if needed
function updatePiscsiGit() {
    cd "$BASE" || exit 1

    set +e
    git rev-parse --is-inside-work-tree &> /dev/null
    if [[ $? -ge 1 ]]; then
        echo "Warning: This does not seem to be a valid clone of a git repository. I will not be able to pull the latest code."
        return 0
    fi
    set -e

    stashed=0
    if [[ $(git diff --stat) != '' ]]; then
        echo "There are local changes to the PiSCSI code; we will stash and reapply them."
        git -c user.name="${GIT_COMMITTER_NAME-piscsi}" -c user.email="${GIT_COMMITTER_EMAIL-piscsi@piscsi.com}" stash
        stashed=1
    fi

    if [[ `git for-each-ref --format='%(upstream:short)' "$(git symbolic-ref -q HEAD)"` != "" ]]; then
        echo "Updating checked out git branch $GIT_REMOTE/$GIT_BRANCH"
        git pull --ff-only
    else
        echo "Detected a local git working branch; skipping the remote update step."
    fi

    if [ $stashed -eq 1 ]; then
        echo "Reapplying local changes..."
        git stash apply
    fi
}

# Takes a backup copy of the piscsi.service file if it exists
function backupPiscsiService() {
    if [ -f "$SYSTEMD_PATH/piscsi.service" ]; then
        sudo mv "$SYSTEMD_PATH/piscsi.service" "$SYSTEMD_PATH/piscsi.service.old"
        SYSTEMD_BACKUP=true
        echo "Existing version of piscsi.service detected; Backing up to piscsi.service.old"
    else
        SYSTEMD_BACKUP=false
    fi
}

# Offers the choice of enabling token-based authentication for PiSCSI, or disables it if enabled
function configureTokenAuth() {
    if [[ -f "$HOME/.rascsi_secret" ]]; then
        sudo mv "$HOME/.rascsi_secret" "$SECRET_FILE"
        echo "Renamed legacy RaSCSI token file for use with PiSCSI"
        return
    fi

    if [[ -f "$CFG_PATH/rascsi_secret" ]]; then
        sudo mv "$CFG_PATH/rascsi_secret" "$SECRET_FILE"
        echo "Renamed legacy RaSCSI token file for use with PiSCSI"
        return
    fi

    if [[ -f $SECRET_FILE ]]; then
        sudo rm "$SECRET_FILE"
        echo "PiSCSI token file $SECRET_FILE already exists. Do you want to disable authentication? (y/N)"
        read REPLY

        if [[ $REPLY =~ ^[Yy]$ ]]; then
            sudo sed -i 's@-P '"$SECRET_FILE"'@@' "$SYSTEMD_PATH/piscsi.service"
            return
        fi
    fi

    echo -n "Enter the token password for protecting PiSCSI: "
    read -r TOKEN

    echo "$TOKEN" > "$SECRET_FILE"

	# Make the secret file owned and only readable by root
    sudo chown root:root "$SECRET_FILE"
    sudo chmod 600 "$SECRET_FILE"

    sudo sed -i "s@^ExecStart.*@& -P $SECRET_FILE@" "$SYSTEMD_PATH/piscsi.service"

    echo ""
    echo "Configured PiSCSI to use $SECRET_FILE for authentication. This file is readable by root only."
    echo "Make note of your password: you will need it to use scsictl and other PiSCSI clients."
    echo "If you have PiSCSI clients installed, please re-run the installation scripts, or update the systemd config manually."
}

# Enables and starts the piscsi service
function enablePiscsiService() {
    sudo systemctl daemon-reload
    sudo systemctl restart rsyslog
    sudo systemctl enable piscsi # start piscsi at boot
    sudo systemctl start piscsi

}

# Modifies and installs the piscsi-web service
function installWebInterfaceService() {
    if [[ -f "$SECRET_FILE" && -z "$TOKEN" ]] ; then
        echo ""
        echo "Secret token file $SECRET_FILE detected. You must enter the password, or press Ctrl+C to cancel installation."
        read -r TOKEN
    fi

    echo "Installing the piscsi-web.service configuration..."
    sudo cp -f "$WEB_INSTALL_PATH/service-infra/piscsi-web.service" "$SYSTEMD_PATH/piscsi-web.service"
    sudo sed -i /^ExecStart=/d "$SYSTEMD_PATH/piscsi-web.service"

    if [ ! -z "$TOKEN" ]; then
        sudo sed -i "8 i ExecStart=$WEB_INSTALL_PATH/start.sh --password=$TOKEN" "$SYSTEMD_PATH/piscsi-web.service"
	# Make the service file readable by root only, to protect the token string
        sudo chmod 600 "$SYSTEMD_PATH/piscsi-web.service"
        echo "Granted access to the Web Interface with the token password that you configured for PiSCSI."
    else
        sudo sed -i "8 i ExecStart=$WEB_INSTALL_PATH/start.sh" "$SYSTEMD_PATH/piscsi-web.service"
    fi

    sudo systemctl daemon-reload
    sudo systemctl enable piscsi-web
    sudo systemctl start piscsi-web
}

# Checks for and disables legacy systemd services
function migrateLegacyData() {
    if [[ -f "$SYSTEMD_PATH/rascsi.service" ]]; then
        stopService "rascsi"
        disableService "rascsi"
        sudo mv "$SYSTEMD_PATH/rascsi.service" "$SYSTEMD_PATH/piscsi.service"
        echo "Renamed rascsi.service to piscsi.service"
    fi
    if [[ -f "$SYSTEMD_PATH/rascsi-web.service" ]]; then
        stopService "rascsi-web"
        disableService "rascsi-web"
        sudo mv "$SYSTEMD_PATH/rascsi-web.service" "$SYSTEMD_PATH/piscsi-web.service"
        echo "Renamed rascsi-web.service to piscsi-web.service"
    fi
    if [[ -f "$SYSTEMD_PATH/rascsi-oled.service" ]]; then
        stopService "rascsi-oled"
        disableService "rascsi-oled"
        sudo mv "$SYSTEMD_PATH/rascsi-oled.service" "$SYSTEMD_PATH/piscsi-oled.service"
        echo "Renamed rascsi-oled.service to piscsi-oled.service"
    elif [[ -f "$SYSTEMD_PATH/monitor_rascsi.service" ]]; then
        stopService "monitor_rascsi"
        disableService "monitor_rascsi"
        sudo mv "$SYSTEMD_PATH/monitor_rascsi.service" "$SYSTEMD_PATH/piscsi-oled.service"
        echo "Renamed monitor_rascsi.service to piscsi-oled.service"
    fi
    if [[ -f "$SYSTEMD_PATH/rascsi-ctrlboard.service" ]]; then
        stopService "rascsi-ctrlboard"
        disableService "rascsi-ctrlboard"
        sudo mv "$SYSTEMD_PATH/rascsi-ctrlboard.service" "$SYSTEMD_PATH/piscsi-ctrlboard.service"
        echo "Renamed rascsi-ctrlboard.service to piscsi-ctrlboard.service"
    fi
    if [[ -f "/etc/rsyslog.d/rascsi.conf" ]]; then
        sudo rm "/etc/rsyslog.d/rascsi.conf"
        sudo cp "$BASE/os_integration/piscsi.conf" "/etc/rsyslog.d"
        echo "Replaced rascsi.conf with piscsi.conf"
    fi
    if [[ -f "/etc/network/interfaces.d/rascsi_bridge" ]]; then
        sudo rm "/etc/network/interfaces.d/rascsi_bridge"
        sudo cp "$BASE/os_integration/piscsi_bridge" "/etc/network/interfaces.d"
        echo "Replaced rascsi_bridge with piscsi_bridge"
    fi
    if [[ $(getent group rascsi) && $(getent group "$AUTH_GROUP") ]]; then
        sudo groupdel rascsi
        echo "Deleted the rascsi group in favor of the existing piscsi group"
    elif [ $(getent group rascsi) ]; then
        sudo groupmod --new-name piscsi rascsi
        echo "Renamed the rascsi group to piscsi"
    fi
}

# Stops a service if it is running
function stopService() {
    if [[ -f "$SYSTEMD_PATH/$1.service" ]]; then
        SERVICE_RUNNING=0
        sudo systemctl is-active --quiet "$1.service" >/dev/null 2>&1 || SERVICE_RUNNING=$?
        if [[ $SERVICE_RUNNING -eq 0 ]]; then
            sudo systemctl stop "$1.service"
        fi
    fi
}

# disables a service if it is enabled
function disableService() {
    if [ -f "$SYSTEMD_PATH/$1.service" ]; then
        SERVICE_ENABLED=0
        sudo systemctl is-enabled --quiet "$1.service" >/dev/null 2>&1 || SERVICE_ENABLED=$?
        if [[ $SERVICE_ENABLED -eq 0 ]]; then
          sudo systemctl disable "$1.service"
        fi
    fi
}

# Checks whether the piscsi-oled service is installed
function isPiscsiScreenInstalled() {
    SERVICE_PISCSI_OLED_ENABLED=0
    if [[ -f "$SYSTEMD_PATH/piscsi-oled.service" ]]; then
        sudo systemctl is-enabled --quiet piscsi-oled.service >/dev/null 2>&1 || SERVICE_PISCSI_OLED_ENABLED=$?
    else
        SERVICE_PISCSI_OLED_ENABLED=1
    fi

    echo $SERVICE_PISCSI_OLED_ENABLED
}

# Checks whether the piscsi-ctrlboard service is installed
function isPiscsiCtrlBoardInstalled() {
    SERVICE_PISCSI_CTRLBOARD_ENABLED=0
    if [[ -f "$SYSTEMD_PATH/piscsi-ctrlboard.service" ]]; then
        sudo systemctl is-enabled --quiet piscsi-ctrlboard.service >/dev/null 2>&1 || SERVICE_PISCSI_CTRLBOARD_ENABLED=$?
    else
        SERVICE_PISCSI_CTRLBOARD_ENABLED=1
    fi

    echo $SERVICE_PISCSI_CTRLBOARD_ENABLED
}

# Checks whether the piscsi-oled service is running
function isPiscsiScreenRunning() {
    SERVICE_PISCSI_OLED_RUNNING=0
    if [[ -f "$SYSTEMD_PATH/piscsi-oled.service" ]]; then
        sudo systemctl is-active --quiet piscsi-oled.service >/dev/null 2>&1 || SERVICE_PISCSI_OLED_RUNNING=$?
    else
        SERVICE_PISCSI_OLED_RUNNING=1
    fi

    echo $SERVICE_PISCSI_OLED_RUNNING
}

# Checks whether the piscsi-oled service is running
function isPiscsiCtrlBoardRunning() {
    SERVICE_PISCSI_CTRLBOARD_RUNNING=0
    if [[ -f "$SYSTEMD_PATH/piscsi-ctrlboard.service" ]]; then
        sudo systemctl is-active --quiet piscsi-ctrlboard.service >/dev/null 2>&1 || SERVICE_PISCSI_CTRLBOARD_RUNNING=$?
    else
        SERVICE_PISCSI_CTRLBOARD_RUNNING=1
    fi

    echo $SERVICE_PISCSI_CTRLBOARD_RUNNING
}


# Starts the piscsi-oled service if installed
function startPiscsiScreen() {
    if [[ $(isPiscsiScreenInstalled) -eq 0 ]] && [[ $(isPiscsiScreenRunning) -ne 1 ]]; then
        sudo systemctl start piscsi-oled.service
        showServiceStatus "piscsi-oled"
    fi
}

# Starts the piscsi-ctrlboard service if installed
function startPiscsiCtrlBoard() {
    if [[ $(isPiscsiCtrlBoardInstalled) -eq 0 ]] && [[ $(isPiscsiCtrlBoardRunning) -ne 1 ]]; then
        sudo systemctl start piscsi-ctrlboard.service
        showServiceStatus "piscsi-ctrlboard"
    fi
}

# Starts the macproxy service if installed
function startMacproxy() {
    if [ -f "$SYSTEMD_PATH/macproxy.service" ]; then
        sudo systemctl start macproxy.service
        showServiceStatus "macproxy"
    fi
}

# Shows status for the piscsi service
function showServiceStatus() {
    systemctl status "$1.service" | tee
}

# Clone, compile and install 'hfdisk', partition tool
function installHfdisk() {
    HFDISK_VERSION="2022.11"
    if [ ! -x "$HFDISK_BIN" ]; then
        cd "$BASE" || exit 1
        wget -O "hfdisk-$HFDISK_VERSION.tar.gz" "https://github.com/rdmark/hfdisk/archive/refs/tags/$HFDISK_VERSION.tar.gz" </dev/null
        tar -xzvf "hfdisk-$HFDISK_VERSION.tar.gz"
        rm "hfdisk-$HFDISK_VERSION.tar.gz"
        cd "hfdisk-$HFDISK_VERSION" || exit 1
        make

        sudo cp hfdisk "$HFDISK_BIN"

        echo "Installed $HFDISK_BIN"
    fi
}

# Fetch HFS drivers that the Web Interface uses
function fetchHardDiskDrivers() {
    DRIVER_ARCHIVE="mac-hard-disk-drivers"
    if [ ! -d "$BASE/$DRIVER_ARCHIVE" ]; then
        cd "$BASE" || exit 1
        # -N option overwrites if downloaded file is newer than existing file
        wget -N "https://www.dropbox.com/s/gcs4v5pcmk7rxtb/$DRIVER_ARCHIVE.zip?dl=1" -O "$DRIVER_ARCHIVE.zip"
        unzip -d "$DRIVER_ARCHIVE" "$DRIVER_ARCHIVE.zip"
        rm "$DRIVER_ARCHIVE.zip"
    fi
}

# Modifies system configurations for a wired network bridge
function setupWiredNetworking() {
    echo "Setting up wired network..."

    if [[ -z $HEADLESS ]]; then
	LAN_INTERFACE=`ip -o addr show scope link | awk '{split($0, a); print $2}' | grep 'eth\|enx' | head -n 1`
    else
	LAN_INTERFACE="eth0"
    fi

    if [[ -z "$LAN_INTERFACE" ]]; then
	echo "No usable wired network interfaces detected. Have you already enabled the bridge? Aborting..."
	return 1
    fi

    echo "Network interface '$LAN_INTERFACE' will be configured for network forwarding with DHCP."
    echo ""
    echo "WARNING: If you continue, the IP address of your Pi may change upon reboot."
    echo "Please make sure you will not lose access to the Pi system."
    echo ""

    if [[ -z $HEADLESS ]]; then
        echo "Do you want to proceed with network configuration using the default settings? [Y/n]"
        read REPLY

        if [ "$REPLY" == "N" ] || [ "$REPLY" == "n" ]; then
            echo "Available wired interfaces on this system:"
            echo `ip -o addr show scope link | awk '{split($0, a); print $2}' | grep 'eth\|enx'`
            echo "Please type the wired interface you want to use and press Enter:"
            read SELECTED
            LAN_INTERFACE=$SELECTED
        fi
    fi

    if [ "$(grep -c "^denyinterfaces" /etc/dhcpcd.conf)" -ge 1 ]; then
        echo "WARNING: Network forwarding may already have been configured. Proceeding will overwrite the configuration."

        if [[ -z $HEADLESS ]]; then
            echo "Press enter to continue or CTRL-C to exit"
            read REPLY
        fi

        sudo sed -i /^denyinterfaces/d /etc/dhcpcd.conf
    fi

    sudo bash -c 'echo "denyinterfaces '$LAN_INTERFACE'" >> /etc/dhcpcd.conf'
    echo "Modified /etc/dhcpcd.conf"

    # default config file is made for eth0, this will set the right net interface
    sudo bash -c 'sed s/eth0/'"$LAN_INTERFACE"'/g '"$BASE"'/os_integration/piscsi_bridge > /etc/network/interfaces.d/piscsi_bridge'
    echo "Modified /etc/network/interfaces.d/piscsi_bridge"

    echo "Configuration completed!"
    echo "Please make sure you attach a DaynaPORT network adapter to your PiSCSI configuration."
    echo "Either use the Web UI, or do this on the command line (assuming SCSI ID 6):"
    echo "scsictl -i 6 -c attach -t scdp -f $LAN_INTERFACE"
    echo ""

    if [[ $HEADLESS ]]; then
        echo "Skipping reboot in headless mode"
        return 0
    fi

    echo "We need to reboot your Pi"
    echo "Press Enter to reboot or CTRL-C to exit"
    read

    echo "Rebooting..."
    sleep 3
    sudo reboot
}

# Modifies system configurations for a wireless network bridge with NAT
function setupWirelessNetworking() {
    NETWORK="10.10.20"
    IP=$NETWORK.2 # Macintosh or Device IP
    NETWORK_MASK="255.255.255.0"
    CIDR="24"
    ROUTER_IP=$NETWORK.1
    ROUTING_ADDRESS=$NETWORK.0/$CIDR

    if [[ -z $HEADLESS ]]; then
	WLAN_INTERFACE=`ip -o addr show scope link | awk '{split($0, a); print $2}' | grep 'wlan\|wlx' | head -n 1`
    else
	WLAN_INTERFACE="wlan0"
    fi

    if [[ -z "$WLAN_INTERFACE" ]]; then
	echo "No usable wireless network interfaces detected. Have you already enabled the bridge? Aborting..."
	return 1
    fi

    echo "Network interface '$WLAN_INTERFACE' will be configured for network forwarding with static IP assignment."
    echo "Configure your Macintosh or other device with the following:"
    echo "IP Address (static): $IP"
    echo "Router Address: $ROUTER_IP"
    echo "Subnet Mask: $NETWORK_MASK"
    echo "DNS Server: Any public DNS server"
    echo ""
    echo "Do you want to proceed with network configuration using the default settings? [Y/n]"
    read REPLY

    if [ "$REPLY" == "N" ] || [ "$REPLY" == "n" ]; then
        echo "Available wireless interfaces on this system:"
        echo `ip -o addr show scope link | awk '{split($0, a); print $2}' | grep 'wlan\|wlx'`
        echo "Please type the wireless interface you want to use and press Enter:"
        read -r WLAN_INTERFACE
        echo "Base IP address (ex. 10.10.20):"
        read -r NETWORK
        echo "CIDR for Subnet Mask (ex. '24' for 255.255.255.0):"
        read -r CIDR
        ROUTER_IP=$NETWORK.1
        ROUTING_ADDRESS=$NETWORK.0/$CIDR
    fi


    if [ "$(grep -c "^net.ipv4.ip_forward=1" /etc/sysctl.conf)" -ge 1 ]; then
        echo "WARNING: Network forwarding may already have been configured. Proceeding will overwrite the configuration."
        echo "Press enter to continue or CTRL-C to exit"
        read REPLY
    else
        sudo bash -c 'echo "net.ipv4.ip_forward=1" >> /etc/sysctl.conf'
        echo "Modified /etc/sysctl.conf"
    fi

    # Check if iptables is installed
    if [ `apt-cache policy iptables | grep Installed | grep -c "(none)"` -eq 0 ]; then
        echo "iptables is already installed"
    else
        sudo apt-get install iptables --assume-yes --no-install-recommends </dev/null
    fi

    sudo iptables --flush
    sudo iptables -t nat -F
    sudo iptables -X
    sudo iptables -Z
    sudo iptables -P INPUT ACCEPT
    sudo iptables -P OUTPUT ACCEPT
    sudo iptables -P FORWARD ACCEPT
    sudo iptables -t nat -A POSTROUTING -o "$WLAN_INTERFACE" -s "$ROUTING_ADDRESS" -j MASQUERADE

    # Check if iptables-persistent is installed
    if [ `apt-cache policy iptables-persistent | grep Installed | grep -c "(none)"` -eq 0 ]; then
        echo "iptables-persistent is already installed"
        sudo iptables-save --file /etc/iptables/rules.v4
    else
        sudo apt-get install iptables-persistent --assume-yes --no-install-recommends </dev/null
    fi
    echo "Modified /etc/iptables/rules.v4"

    echo "Configuration completed!"
    echo ""
    echo "Please make sure you attach a DaynaPORT network adapter to your PiSCSI configuration"
    echo "Either use the Web UI, or do this on the command line (assuming SCSI ID 6):"
    echo "scsictl -i 6 -c attach -t scdp -f $WLAN_INTERFACE:$ROUTER_IP/$CIDR"
    echo ""
    echo "We need to reboot your Pi"
    echo "Press Enter to reboot or CTRL-C to exit"
    read REPLY

    echo "Rebooting..."
    sleep 3
    sudo reboot
}

# Detects or creates the file sharing directory
function createFileSharingDir() {
    if [ ! -d "$FILE_SHARE_PATH" ] && [ -d "$HOME/afpshare" ]; then
        echo
        echo "File server dir $HOME/afpshare detected. This script will rename it to $FILE_SHARE_PATH."
        echo
        echo "Do you want to proceed with the installation? [y/N]"
        read -r REPLY
        if [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
            sudo mv "$HOME/afpshare" "$FILE_SHARE_PATH" || exit 1
        else
            exit 0
        fi
    elif [ -d "$FILE_SHARE_PATH" ]; then
        echo "Found a $FILE_SHARE_PATH directory; will use it for file sharing."
    else
        echo "Creating the $FILE_SHARE_PATH directory and granting read/write permissions to all users..."
        sudo mkdir -p "$FILE_SHARE_PATH"
        sudo chown -R "$USER:$USER" "$FILE_SHARE_PATH"
        chmod -Rv 775 "$FILE_SHARE_PATH"
    fi
}

# Downloads, compiles, and installs Netatalk (AppleShare server)
function installNetatalk() {
    NETATALK_VERSION="230701"
    NETATALK_CONFIG_PATH="/etc/netatalk"
    NETATALK_OPTIONS="--cores=$CORES --share-name='$FILE_SHARE_NAME' --share-path='$FILE_SHARE_PATH'"

    if [ -d "$NETATALK_CONFIG_PATH" ]; then
        echo
        echo "WARNING: Netatalk configuration dir $NETATALK_CONFIG_PATH already exists."
        echo "This installation process will overwrite existing binaries and configurations."
        echo "No shared files will be deleted, but you may have to manually restore your settings after the installation."
        echo
        echo "Do you want to proceed with the installation? [y/N]"
        read -r REPLY
        if ! [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
            exit 0
        fi
    fi

    echo
    echo "Downloading tarball to $HOME..."
    cd $HOME || exit 1
    wget -O "netatalk-2.$NETATALK_VERSION.tar.gz" "https://github.com/rdmark/netatalk-2.x/releases/download/netatalk-2-$NETATALK_VERSION/netatalk-2.$NETATALK_VERSION.tar.gz" </dev/null

    echo "Unpacking tarball..."
    tar -xzf "netatalk-2.$NETATALK_VERSION.tar.gz"
    rm "netatalk-2.$NETATALK_VERSION.tar.gz"

    if [ -f "/etc/network/interfaces.d/piscsi_bridge" ]; then
        echo "PiSCSI network bridge detected. Using 'piscsi_bridge' interface for AppleTalk."
        NETATALK_OPTIONS="$NETATALK_OPTIONS --appletalk-interface=piscsi_bridge"
    fi

    [[ $HEADLESS ]] && NETATALK_OPTIONS="$NETATALK_OPTIONS --headless"
    [[ $SKIP_PACKAGES ]] && NETATALK_OPTIONS="$NETATALK_OPTIONS --no-packages"
    [[ $SKIP_MAKE_CLEAN ]] && NETATALK_OPTIONS="$NETATALK_OPTIONS --no-make-clean"

    cd "$HOME/netatalk-2.$NETATALK_VERSION/contrib/shell_utils" || exit 1
    bash -c "./debian_install.sh $NETATALK_OPTIONS" || exit 1
}

# Appends the images dir as a shared Netatalk volume
function shareImagesWithNetatalk() {
    APPLEVOLUMES_PATH="/etc/netatalk/AppleVolumes.default"
    if ! [ -f "$APPLEVOLUMES_PATH" ]; then
        echo "Could not find $APPLEVOLUMES_PATH ... is Netatalk installed?"
        exit 1
    fi

    if [ "$(grep -c "$VIRTUAL_DRIVER_PATH" "$APPLEVOLUMES_PATH")" -ge 1 ]; then
        echo "The $VIRTUAL_DRIVER_PATH dir is already shared in $APPLEVOLUMES_PATH"
        echo "Do you want to turn off the sharing? [y/N]"
        read -r REPLY
        if [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
            sudo systemctl stop afpd
            sudo sed -i '\,^'"$VIRTUAL_DRIVER_PATH"',d' "$APPLEVOLUMES_PATH"
            echo "Sharing for $VIRTUAL_DRIVER_PATH disabled!"
            sudo systemctl start afpd
            exit 0
        fi
        exit 0
    fi

    sudo systemctl stop afpd
    echo "Appended to AppleVolumes.default:"
    echo "$VIRTUAL_DRIVER_PATH \"PiSCSI Images\"" | sudo tee -a "$APPLEVOLUMES_PATH"
    sudo systemctl start afpd

    echo
    echo "WARNING: Do not inadvertently move or rename image files that are in use by PiSCSI."
    echo "Doing so may lead to data loss."
    echo
}

# Downloads, compiles, and installs Macproxy (web proxy)
function installMacproxy {
    PORT=5000

    echo "Macproxy is a Web Proxy for all vintage Web Browsers -- not only for Macs!"
    echo ""
    echo "By default, Macproxy listens to port $PORT, but you can choose any other available port."
    echo -n "Enter a port number 1024 - 65535, or press Enter to use the default port: "

    read -r CHOICE
    if [[ $CHOICE -ge "1024" ]] && [[ $CHOICE -le "65535" ]]; then
        PORT=$CHOICE
    else
        echo "Using the default port $PORT"
    fi

    if [[ $SKIP_PACKAGES ]]; then
        echo "Skipping package installation"
    else
        sudo apt-get update && sudo apt-get install python3 python3-venv --assume-yes --no-install-recommends </dev/null
    fi

    MACPROXY_VER="22.8"
    MACPROXY_PATH="$HOME/macproxy-$MACPROXY_VER"
    if [ -d "$MACPROXY_PATH" ]; then
        echo "The $MACPROXY_PATH directory already exists. Deleting before downloading again..."
        sudo rm -rf "$MACPROXY_PATH"
    fi
    cd "$HOME" || exit 1
    wget -O "macproxy-$MACPROXY_VER.tar.gz" "https://github.com/rdmark/macproxy/archive/refs/tags/v$MACPROXY_VER.tar.gz" </dev/null
    tar -xzvf "macproxy-$MACPROXY_VER.tar.gz"

    sudo cp "$MACPROXY_PATH/macproxy.service" "$SYSTEMD_PATH"
    sudo sed -i /^ExecStart=/d "$SYSTEMD_PATH/macproxy.service"
    sudo sed -i "8 i ExecStart=$MACPROXY_PATH/start_macproxy.sh -p=$PORT" "$SYSTEMD_PATH/macproxy.service"
    sudo systemctl daemon-reload
    sudo systemctl enable macproxy
    startMacproxy

    echo -n "Macproxy is now running on IP "
    echo -n `ip -4 addr show scope global | grep -o -m 1 -P '(?<=inet\s)\d+(\.\d+){3}'`
    echo " port $PORT"
    echo "Configure your browser to use the above as http (and https) proxy."
    echo ""
}

# Installs vsftpd (FTP server)
function installFtp() {
    echo
    echo "Installing packages..."
    sudo apt-get update && sudo apt-get install vsftpd --assume-yes --no-install-recommends </dev/null

    echo
    echo "Connect to the FTP server with:"
    echo -n "ftp://"
    echo -n `ip -4 addr show scope global | grep -o -m 1 -P '(?<=inet\s)\d+(\.\d+){3}'`
    echo "/"
    echo
    echo "Authenticate with username '$USER' and your password on this Pi."
    echo
}

# Installs and configures Samba (SMB server)
function installSamba() {
    SAMBA_CONFIG_PATH="/etc/samba"

    if [ -d "$SAMBA_CONFIG_PATH" ]; then
        echo
        echo "Samba configuration dir $SAMBA_CONFIG_PATH already exists."
        echo "This installation process may overwrite existing binaries and configurations."
        echo "No shared files will be deleted, but you may have to manually restore your settings after the installation."
        echo
        echo "Do you want to proceed with the installation? [y/N]"
        read -r REPLY
        if ! [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
            exit 0
        fi
    fi

    echo
    echo "Installing packages..."
    sudo apt-get update || true
    sudo apt-get install samba --no-install-recommends --assume-yes </dev/null
    echo
    echo "Modifying $SAMBA_CONFIG_PATH/smb.conf ..."
    if [[ `sudo grep -c "server min protocol = NT1" $SAMBA_CONFIG_PATH/smb.conf` -eq 0 ]]; then
        # Allow Windows XP clients and earlier to connect to the server
        sudo sed -i 's/\[global\]/\[global\]\nserver min protocol = NT1/' "$SAMBA_CONFIG_PATH/smb.conf"
        echo "server min prototol = NT1"
    fi
    if [[ `sudo grep -c "\[Pi File Server\]" $SAMBA_CONFIG_PATH/smb.conf` -eq 0 ]]; then
        # Define a shared directory with full read/write privileges, while aggressively hiding dot files
        echo -e '\n[Pi File Server]\npath = '"$FILE_SHARE_PATH"'\nbrowseable = yes\nwriteable = yes\nhide dot files = yes\nveto files = /.*/' | sudo tee -a "$SAMBA_CONFIG_PATH/smb.conf"
    fi

    sudo systemctl restart smbd

    echo "Please create a Samba password for user $USER"
    sudo smbpasswd -a "$USER"
}

# Installs and configures Webmin
function installWebmin() {
    WEBMIN_PATH="/usr/share/webmin"
    WEBMIN_MODULE_CONFIG="/etc/webmin/netatalk2/config"
    WEBMIN_MODULE_VERSION="1.0"

    if [ -d "$WEBMIN_PATH" ]; then
        echo
        echo "Webmin dir $WEBMIN_PATH already exists."
        echo "This installation process may overwrite existing software."
        echo
        echo "Do you want to proceed with the installation? [y/N]"
        read -r REPLY
        if ! [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
            exit 0
        fi
    fi

    echo
    echo "Installing packages..."
    sudo apt-get install curl libcgi-session-perl --no-install-recommends --assume-yes </dev/null
    curl -o setup-repos.sh https://raw.githubusercontent.com/webmin/webmin/master/setup-repos.sh
    sudo sh setup-repos.sh -f
    rm setup-repos.sh
    sudo apt-get install webmin --install-recommends --assume-yes </dev/null
    echo
    echo "Downloading and installing Webmin module..."
    if [[ -f "$WEBMIN_MODULE_CONFIG" ]]; then
        echo "$WEBMIN_MODULE_CONFIG already exists; will not modify..."
	WEBMIN_MODULE_FLAG=1
    fi

    rm netatalk2-wbm.tgz 2> /dev/null || true
    wget -O netatalk2-wbm.tgz "https://github.com/Netatalk/netatalk-webmin/releases/download/netatalk2-$WEBMIN_MODULE_VERSION/netatalk2-wbm-$WEBMIN_MODULE_VERSION.tgz" </dev/null
    sudo "$WEBMIN_PATH/install-module.pl" netatalk2-wbm.tgz

    if [[ ! $WEBMIN_MODULE_FLAG ]]; then
        echo "Modifying $WEBMIN_MODULE_CONFIG..."
        sudo sed -i 's@/usr/sbin@/usr/local/sbin@' "$WEBMIN_MODULE_CONFIG"
    fi
    rm netatalk2-wbm.tgz || true
}

# updates configuration files and installs packages needed for the OLED screen script
function installPiscsiScreen() {
    if [[ -f "$SECRET_FILE" && -z "$TOKEN" ]] ; then
        echo ""
        echo "Secret token file $SECRET_FILE detected. You must enter the password, or press Ctrl+C to cancel installation."
        read -r TOKEN
    fi

    echo "IMPORTANT: This configuration requires a OLED screen to be installed onto your PiSCSI board."
    echo "See wiki for more information: https://github.com/akuker/PISCSI/wiki/OLED-Status-Display-(Optional)"
    echo ""
    echo "Choose screen rotation:"
    echo "  1) 0 degrees"
    echo "  2) 180 degrees (default)"
    read REPLY

    if [ "$REPLY" == "1" ]; then
        echo "Proceeding with 0 degrees rotation."
        ROTATION="0"
    else
        echo "Proceeding with 180 degrees rotation."
        ROTATION="180"
    fi

    echo ""
    echo "Choose screen resolution:"
    echo "  1) 128x32 pixels (default)"
    echo "  2) 128x64 pixels"
    read REPLY

    if [ "$REPLY" == "2" ]; then
        echo "Proceeding with 128x64 pixel resolution."
        SCREEN_HEIGHT="64"
    else
        echo "Proceeding with 128x32 pixel resolution."
        SCREEN_HEIGHT="32"
    fi

    stopService "piscsi-oled"
    stopService "piscsi-ctrlboard"
    disableService "piscsi-ctrlboard"
    updatePiscsiGit

    if [[ $SKIP_PACKAGES ]]; then
        echo "Skipping package installation"
    else
        sudo apt-get update && sudo apt-get install libjpeg-dev libpng-dev libopenjp2-7-dev i2c-tools raspi-config --assume-yes --no-install-recommends </dev/null
    fi

    if [[ $(grep -c "^dtparam=i2c_arm=on" /boot/config.txt) -ge 1 ]]; then
        echo "NOTE: I2C support seems to have been configured already."
        REBOOT=0
    else
        sudo raspi-config nonint do_i2c 0 </dev/null
        echo "Modified the Raspberry Pi boot configuration to enable I2C."
        echo "A reboot will be required for the change to take effect."
        REBOOT=1
    fi

    # Deleting previous venv dir, if one exists, to avoid the common issue of broken python dependencies
    deleteDir "$OLED_INSTALL_PATH/venv"

    echo "Installing the piscsi-oled.service configuration..."
    sudo cp -f "$OLED_INSTALL_PATH/service-infra/piscsi-oled.service" "$SYSTEMD_PATH/piscsi-oled.service"
    sudo sed -i /^ExecStart=/d "$SYSTEMD_PATH/piscsi-oled.service"
    if [ ! -z "$TOKEN" ]; then
        sudo sed -i "8 i ExecStart=$OLED_INSTALL_PATH/start.sh --rotation=$ROTATION --height=$SCREEN_HEIGHT --password=$TOKEN" "$SYSTEMD_PATH/piscsi-oled.service"
	# Make the service file readable by root only, to protect the token string
        sudo chmod 600 "$SYSTEMD_PATH/piscsi-oled.service"
        echo "Granted access to the OLED Monitor with the password that you configured for PiSCSI."
    else
        sudo sed -i "8 i ExecStart=$OLED_INSTALL_PATH/start.sh --rotation=$ROTATION --height=$SCREEN_HEIGHT" "$SYSTEMD_PATH/piscsi-oled.service"
    fi

    sudo systemctl daemon-reload

    sudo systemctl daemon-reload
    sudo systemctl enable piscsi-oled

    if [ $REBOOT -eq 1 ]; then
        echo ""
        echo "The piscsi-oled service will start on the next Pi boot."
        echo "Press Enter to reboot or CTRL-C to exit"
        read

        echo "Rebooting..."
        sleep 3
        sudo reboot
    fi

    sudo systemctl start piscsi-oled
}

# updates configuration files and installs packages needed for the CtrlBoard script
function installPiscsiCtrlBoard() {
    if [[ -f "$SECRET_FILE" && -z "$TOKEN" ]] ; then
        echo ""
        echo "Secret token file $SECRET_FILE detected. You must enter the password, or press Ctrl+C to cancel installation."
        read -r TOKEN
    fi

    echo "IMPORTANT: This configuration requires a PiSCSI Control Board connected to your PiSCSI board."
    echo "See wiki for more information: https://github.com/akuker/PISCSI/wiki/PiSCSI-Control-Board"
    echo ""
    echo "Choose screen rotation:"
    echo "  1) 0 degrees"
    echo "  2) 180 degrees (default)"
    read REPLY

    if [ "$REPLY" == "1" ]; then
        echo "Proceeding with 0 degrees rotation."
        ROTATION="0"
    else
        echo "Proceeding with 180 degrees rotation."
        ROTATION="180"
    fi

    stopService "piscsi-ctrlboard"
    updatePiscsiGit

    if [[ $SKIP_PACKAGES ]]; then
        echo "Skipping package installation"
    else
        sudo apt-get update && sudo apt-get install libjpeg-dev libpng-dev libopenjp2-7-dev i2c-tools raspi-config --assume-yes --no-install-recommends </dev/null
        # install python packages through apt that need compilation
        sudo apt-get install python3-cbor2 --assume-yes --no-install-recommends </dev/null
    fi

    # enable i2c
    if [[ $(grep -c "^dtparam=i2c_arm=on" /boot/config.txt) -ge 1 ]]; then
        echo "NOTE: I2C support seems to have been configured already."
        REBOOT=0
    else
        sudo raspi-config nonint do_i2c 0 </dev/null
        echo "Modified the Raspberry Pi boot configuration to enable I2C."
        echo "A reboot will be required for the change to take effect."
        REBOOT=1
    fi

    # determine target baudrate
    PI_MODEL=$(/usr/bin/tr -d '\0' < /proc/device-tree/model)
    TARGET_I2C_BAUDRATE=100000
    if [[ ${PI_MODEL} =~ "Raspberry Pi 4" ]]; then
      echo "Detected: Raspberry Pi 4"
      TARGET_I2C_BAUDRATE=1000000
    elif [[ ${PI_MODEL} =~ "Raspberry Pi 3" ]] || [[ ${PI_MODEL} =~ "Raspberry Pi Zero 2" ]]; then
      echo "Detected: Raspberry Pi 3 or Zero 2"
      TARGET_I2C_BAUDRATE=400000
    else
      echo "No Raspberry Pi 4, Pi 3 or Pi Zero 2 detected. Falling back on low i2c baudrate."
      echo "Transition animations will be disabled."
    fi

    # adjust i2c baudrate according to the raspberry pi model detection
    set +e
    GREP_PARAM="^dtparam=i2c_arm=on,i2c_arm_baudrate=${TARGET_I2C_BAUDRATE}$"
    ADJUST_BAUDRATE=$(grep -c "${GREP_PARAM}" /boot/config.txt)
    if [[ $ADJUST_BAUDRATE -eq 0 ]]; then
      echo "Adjusting I2C baudrate in /boot/config.txt"
      sudo sed -i "s/dtparam=i2c_arm=on.*/dtparam=i2c_arm=on,i2c_arm_baudrate=${TARGET_I2C_BAUDRATE}/g" /boot/config.txt
      REBOOT=1
    else
      echo "I2C baudrate already correct in /boot/config.txt"
    fi
    set -e

    # Deleting previous venv dir, if one exists, to avoid the common issue of broken python dependencies
    deleteDir "$CTRLBOARD_INSTALL_PATH/venv"

    echo "Installing the piscsi-ctrlboard.service configuration..."
    sudo cp -f "$CTRLBOARD_INSTALL_PATH/service-infra/piscsi-ctrlboard.service" "$SYSTEMD_PATH/piscsi-ctrlboard.service"
    sudo sed -i /^ExecStart=/d "$SYSTEMD_PATH/piscsi-ctrlboard.service"
    if [ ! -z "$TOKEN" ]; then
        sudo sed -i "8 i ExecStart=$CTRLBOARD_INSTALL_PATH/start.sh --rotation=$ROTATION --password=$TOKEN" "$SYSTEMD_PATH/piscsi-ctrlboard.service"
        sudo chmod 600 "$SYSTEMD_PATH/piscsi-ctrlboard.service"
        echo "Granted access to the PiSCSI Control Board UI with the password that you configured for PiSCSI."
    else
        sudo sed -i "8 i ExecStart=$CTRLBOARD_INSTALL_PATH/start.sh --rotation=$ROTATION" "$SYSTEMD_PATH/piscsi-ctrlboard.service"
    fi

    sudo systemctl daemon-reload

    stopService "piscsi-oled"
    disableService "piscsi-oled"

    sudo systemctl daemon-reload
    sudo systemctl enable piscsi-ctrlboard

    if [ $REBOOT -eq 1 ]; then
        echo ""
        echo "The piscsi-ctrlboard service will start on the next Pi boot."
        echo "Press Enter to reboot or CTRL-C to exit"
        read

        echo "Rebooting..."
        sleep 3
        sudo reboot
    fi

    sudo systemctl start piscsi-ctrlboard
}

# Prints a notification if the piscsi.service file was backed up
function notifyBackup {
    if "$SYSTEMD_BACKUP"; then
        echo ""
        echo "IMPORTANT: $SYSTEMD_PATH/piscsi.service has been overwritten."
        echo "A backup copy was saved as piscsi.service.old in the same directory."
        echo "Please inspect the backup file and restore configurations that are important to your setup."
        echo ""
    fi
}

# Creates the group and modifies current user for Web Interface auth
function enableWebInterfaceAuth {
    if [ $(getent group "$AUTH_GROUP") ]; then
        echo "The '$AUTH_GROUP' group already exists."
        echo "Do you want to disable Web Interface authentication? (y/N)"
        read -r REPLY
        if [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
            sudo groupdel "$AUTH_GROUP"
            echo "The '$AUTH_GROUP' group has been deleted."
            exit 0
        fi
    else
        echo "Creating the '$AUTH_GROUP' group."
        sudo groupadd "$AUTH_GROUP"
    fi

    echo "Adding user '$USER' to the '$AUTH_GROUP' group."
    sudo usermod -a -G "$AUTH_GROUP" "$USER"
}

# Executes the keyword driven scripts for a particular action in the main menu
function runChoice() {
  case $1 in
          1)
              echo "Installing / Updating PiSCSI Service ($CONNECT_TYPE) + Web Interface"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              echo "- Install the Nginx web server"
              echo "- Create files and directories"
              echo "- Change permissions of files and directories"
              echo "- Modify user groups and permissions"
              echo "- Install binaries to /usr/local/bin"
              echo "- Install manpages to /usr/local/man"
              echo "- Create a self-signed certificate in /etc/ssl"
              sudoCheck
              createImagesDir
              createCfgDir
              migrateLegacyData
              stopService "piscsi-web"
              updatePiscsiGit
              installPackages
              installHfdisk
              fetchHardDiskDrivers
              stopService "piscsi-ctrlboard"
              stopService "piscsi-oled"
              stopService "piscsi"
              compilePiscsi
              backupPiscsiService
              installPiscsi
              configurePiscsiService
              enablePiscsiService
              preparePythonCommon
              if [[ $(isPiscsiScreenInstalled) -eq 0 ]]; then
                  echo "Detected piscsi oled service; will run the installation steps for the OLED monitor."
                  installPiscsiScreen
              elif [[ $(isPiscsiCtrlBoardInstalled) -eq 0 ]]; then
                  echo "Detected piscsi control board service; will run the installation steps for the control board ui."
                  installPiscsiCtrlBoard
              fi
              cachePipPackages
              installPiscsiWebInterface
              installWebInterfaceService
              showServiceStatus "piscsi-oled"
              showServiceStatus "piscsi-ctrlboard"
              showServiceStatus "piscsi"
              showServiceStatus "piscsi-web"
              notifyBackup
              echo "Installing / Updating PiSCSI Service ($CONNECT_TYPE) + Web Interface - Complete!"
          ;;
          2)
              echo "Installing / Updating PiSCSI Service ($CONNECT_TYPE)"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              echo "- Create files ans directories"
              echo "- Change permissions of files and directories"
              echo "- Modify user groups and permissions"
              echo "- Install binaries to /usr/local/bin"
              echo "- Install manpages to /usr/local/man"
              sudoCheck
              createImagesDir
              createCfgDir
              updatePiscsiGit
              installPackagesStandalone
              migrateLegacyData
              stopService "piscsi-ctrlboard"
              stopService "piscsi-oled"
              stopService "piscsi"
              compilePiscsi
              backupPiscsiService
              preparePythonCommon
              installPiscsi
              configurePiscsiService
              enablePiscsiService
              if [[ $(isPiscsiScreenInstalled) -eq 0 ]]; then
                  echo "Detected piscsi oled service; will run the installation steps for the OLED monitor."
                  installPiscsiScreen
              elif [[ $(isPiscsiCtrlBoardInstalled) -eq 0 ]]; then
                  echo "Detected piscsi control board service; will run the installation steps for the control board ui."
                  installPiscsiCtrlBoard
              fi
              showServiceStatus "piscsi-oled"
              showServiceStatus "piscsi-ctrlboard"
              showServiceStatus "piscsi"
              notifyBackup
              echo "Installing / Updating PiSCSI Service ($CONNECT_TYPE) - Complete!"
          ;;
          3)
              echo "Installing / Updating PiSCSI OLED Screen"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              echo "- Modify the Raspberry Pi boot configuration (may require a reboot)"
              sudoCheck
              preparePythonCommon
              migrateLegacyData
              installPiscsiScreen
              showServiceStatus "piscsi-oled"
              echo "Installing / Updating PiSCSI OLED Screen - Complete!"
          ;;
          4)
              echo "Installing / Updating PiSCSI Control Board UI"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              echo "- Stop and disable the PiSCSI OLED service if it is running"
              echo "- Modify the Raspberry Pi boot configuration (may require a reboot)"
              sudoCheck
              preparePythonCommon
              migrateLegacyData
              installPiscsiCtrlBoard
              showServiceStatus "piscsi-ctrlboard"
              echo "Installing / Updating PiSCSI Control Board UI - Complete!"
          ;;
          5)
              echo "Configuring wired network bridge"
              echo "This script will make the following changes to your system:"
              echo "- Create a virtual network bridge interface in /etc/network/interfaces.d"
              echo "- Modify /etc/dhcpcd.conf to bridge the Ethernet interface (may change the IP address; requires a reboot)"
              sudoCheck
              showMacNetworkWired
              setupWiredNetworking
              echo "Configuring wired network bridge - Complete!"
          ;;
          6)
              echo "Configuring wifi network bridge"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Modify /etc/sysctl.conf to enable IPv4 forwarding"
              echo "- Add NAT rules for the wlan interface (requires a reboot)"
              sudoCheck
              showMacNetworkWireless
              setupWirelessNetworking
              echo "Configuring wifi network bridge - Complete!"
          ;;
          7)
              echo "Installing AppleShare File Server"
              createFileSharingDir
              installNetatalk
              echo "Installing AppleShare File Server - Complete!"
          ;;
          8)
              echo "Installing FTP File Server"
              echo "This script will make the following changes to your system:"
              echo " - Install packages with apt-get"
              echo " - Enable the vsftpd systemd service"
              echo "WARNING: The FTP server may transfer unencrypted data over the network."
              echo "Proceed with this installation only if you are on a private, secure network."
              sudoCheck
              createFileSharingDir
              installFtp
              echo "Installing FTP File Server - Complete!"
          ;;
          9)
              echo "Installing SMB File Server"
              echo "This script will make the following changes to your system:"
              echo " - Install packages with apt-get"
              echo " - Enable Samba systemd services"
              echo " - Create a directory in the current user's home directory where shared files will be stored"
              echo " - Create a Samba user for the current user"
              sudoCheck
              createFileSharingDir
              installSamba
              echo "Installing SMB File Server - Complete!"
          ;;
          10)
              echo "Installing Web Proxy Server"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              sudoCheck
              stopService "macproxy"
              installMacproxy
              echo "Installing Web Proxy Server - Complete!"
          ;;
          11)
              echo "Configuring PiSCSI stand-alone ($CONNECT_TYPE)"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Create directories and change permissions"
              echo "- Install binaries to /usr/local/bin"
              echo "- Install manpages to /usr/local/man"
              sudoCheck
              createImagesDir
              updatePiscsiGit
              installPackagesStandalone
              stopService "piscsi"
              compilePiscsi
              installPiscsi
              echo "Configuring PiSCSI stand-alone ($CONNECT_TYPE) - Complete!"
              echo "Use 'piscsi' to launch PiSCSI, and 'scsictl' to control the running process."
          ;;
          12)
              echo "Configuring PiSCSI Web Interface stand-alone"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              echo "- Modify and enable Apache2 and Nginx web service"
              echo "- Create directories and change permissions"
              echo "- Create a self-signed certificate in /etc/ssl"
              sudoCheck
              createCfgDir
              updatePiscsiGit
              installPackagesWeb
              installHfdisk
              fetchHardDiskDrivers
              preparePythonCommon
              cachePipPackages
              installPiscsiWebInterface
              echo "Configuring PiSCSI Web Interface stand-alone - Complete!"
              echo "Launch the Web Interface with the 'start.sh' script. To use a custom port for the web server: 'start.sh --web-port=8081"
          ;;
          13)
              echo "Enabling or disabling PiSCSI back-end authentication"
              echo "This script will make the following changes to your system:"
              echo "- Modify user groups and permissions"
              sudoCheck
              stopService "piscsi"
              configureTokenAuth
              enablePiscsiService
              echo "Enabling or disabling PiSCSI back-end authentication - Complete!"
          ;;
          14)
              echo "Enabling or disabling Web Interface authentication"
              echo "This script will make the following changes to your system:"
              echo "- Modify user groups and permissions"
              sudoCheck
              enableWebInterfaceAuth
              echo "Enabling or disabling Web Interface authentication - Complete!"
          ;;
          15)
              shareImagesWithNetatalk
              echo "Configuring AppleShare File Server - Complete!"
          ;;
          16)
              installPackagesStandalone
              compilePiscsi
          ;;
          17)
              echo "Install Webmin"
              echo "This script will make the following changes to your system:"
              echo "- Add a 3rd party apt repository"
              echo "- Install and start the Webmin webapp"
	      echo "- Install the netatalk2 Webmin module"
              installWebmin
              echo "Install Webmin - Complete!"
	      echo "The Webmin webapp should now be listening to port 10000 on this system"
          ;;
          99)
              echo "Hidden setup mode for running the pi-gen utility"
              echo "This shouldn't be used by normal users"
              sudoCache
              createImagesDir
              createCfgDir
              updatePiscsiGit
              installPackages
              installHfdisk
              fetchHardDiskDrivers
              compilePiscsi
              installPiscsi
              configurePiscsiService
              enablePiscsiService
              preparePythonCommon
              cachePipPackages
              installPiscsiWebInterface
              installWebInterfaceService
              echo "Automated install of the PiSCSI Service $(CONNECT_TYPE) complete!"
          ;;
          -h|--help|h|help)
              showMenu
          ;;
          *)
              echo "${1} is not a valid option, exiting..."
              exit 1
      esac
}

# Reads and validates the main menu choice
function readChoice() {
   choice=-1

   until [ $choice -ge "1" ] && ([ $choice -eq "99" ] || [ $choice -le "17" ]) ; do
       echo -n "Enter your choice (1-17) or CTRL-C to exit: "
       read -r choice
   done

   runChoice "$choice"
}

# Shows the interactive main menu of the script
function showMenu() {
    echo "Board Type: $CONNECT_TYPE | Compiler: $COMPILER | Compiler Cores: $CORES"
    echo ""
    echo "Choose among the following options:"
    echo "INSTALL/UPDATE PISCSI"
    echo "  1) Install or update PiSCSI Service + Web Interface"
    echo "  2) Install or update PiSCSI Service"
    echo "  3) Install or update PiSCSI OLED Screen (requires hardware)"
    echo "  4) Install or update PiSCSI Control Board UI (requires hardware)"
    echo "NETWORK BRIDGE ASSISTANT"
    echo "  5) Configure network bridge for Ethernet (DHCP)"
    echo "  6) Configure network bridge for WiFi (static IP + NAT)" 
    echo "INSTALL COMPANION APPS"
    echo "  7) Install AppleShare File Server (Netatalk)"
    echo "  8) Install FTP File Server (vsftpd)"
    echo "  9) Install SMB File Server (Samba)"
    echo " 10) Install Web Proxy Server (Macproxy)"
    echo "ADVANCED OPTIONS"
    echo " 11) Compile and install PiSCSI stand-alone"
    echo " 12) Configure the PiSCSI Web Interface stand-alone"
    echo " 13) Enable or disable PiSCSI back-end authentication"
    echo " 14) Enable or disable PiSCSI Web Interface authentication"
    echo "EXPERIMENTAL FEATURES"
    echo " 15) Share the images dir over AppleShare (requires Netatalk)"
    echo " 16) Compile PiSCSI binaries"
    echo " 17) Install Webmin to manage Netatalk and Samba"
}

# parse arguments passed to the script
while [ "$1" != "" ]; do
    PARAM=$(echo "$1" | awk -F= '{print $1}')
    VALUE=$(echo "$1" | awk -F= '{print $2}')
    case $PARAM in
        -c | --connect_type)
            if ! [[ $VALUE =~ ^(FULLSPEC|STANDARD|AIBOM|GAMERNIUM)$ ]]; then
                echo "ERROR: The connect type parameter must have a value of: FULLSPEC, STANDARD, AIBOM or GAMERNIUM"
                exit 1
            fi
            CONNECT_TYPE=$VALUE
            ;;
        -r | --run_choice)
            if ! [[ $VALUE =~ ^[1-9][0-9]?$ && $VALUE -ge 1 && $VALUE -le 16 ]]; then
                echo "ERROR: The run choice parameter must have a numeric value between 1 and 16"
                exit 1
            fi
            RUN_CHOICE=$VALUE
            ;;
        -j | --cores)
            if ! [[ $VALUE =~ ^[1-9][0-9]?$ ]]; then
                echo "ERROR: The cores parameter must have a numeric value of at least 1"
                exit 1
            fi
            CORES=$VALUE
            ;;
        -t | --token)
            if [[ -z $VALUE ]]; then
                echo "ERROR: The token parameter cannot be empty"
                exit 1
            fi
            TOKEN=$VALUE
            ;;
        -h | --headless)
            HEADLESS=1
            ;;
        -g | --with_gcc)
            COMPILER="g++"
            ;;
        -s | --skip_packages)
            SKIP_PACKAGES=1
            ;;
        -l | --skip_make_clean)
            SKIP_MAKE_CLEAN=1
            ;;
        *)
            echo "ERROR: Unknown parameter \"$PARAM\""
            exit 1
            ;;
    esac
    shift
done

showPiSCSILogo
initialChecks

if [ -z "${RUN_CHOICE}" ]; then # RUN_CHOICE is unset, show menu
    showMenu
    readChoice
else
    runChoice "$RUN_CHOICE"
fi
