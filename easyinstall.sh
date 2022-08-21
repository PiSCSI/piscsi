#!/usr/bin/env bash

# BSD 3-Clause License
# Author @sonique6784
# Copyright (c) 2020, sonique6784

function showRaSCSILogo(){
logo="""
    .~~.   .~~.\n
  '. \ ' ' / .'\n
   .╔═══════╗.\n
  : ║|¯¯¯¯¯|║ :\n
 ~ (║|_____|║) ~\n
( : ║ .  __ ║ : )\n
 ~ .╚╦═════╦╝. ~\n
  (  ¯¯¯¯¯¯¯  ) RaSCSI Reloaded Assistant\n
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

USER=$(whoami)
BASE=$(dirname "$(readlink -f "${0}")")
VIRTUAL_DRIVER_PATH="$HOME/images"
CFG_PATH="$HOME/.config/rascsi"
WEB_INSTALL_PATH="$BASE/python/web"
OLED_INSTALL_PATH="$BASE/python/oled"
CTRLBOARD_INSTALL_PATH="$BASE/python/ctrlboard"
PYTHON_COMMON_PATH="$BASE/python/common"
SYSTEMD_PATH="/etc/systemd/system"
SSL_CERTS_PATH="/etc/ssl/certs"
SSL_KEYS_PATH="/etc/ssl/private"
HFS_FORMAT=/usr/bin/hformat
HFDISK_BIN=/usr/bin/hfdisk
LIDO_DRIVER=$BASE/lido-driver.img
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
GIT_REMOTE=${GIT_REMOTE:-origin}
TOKEN=""

set -e

# checks to run before entering the script main menu
function initialChecks() {
    if [ "root" == "$USER" ]; then
        echo "Do not run this script as $USER or with 'sudo'."
        exit 1
    fi
}

# checks that the current user has sudoers privileges
function sudoCheck() {
    echo "Input your password to allow this script to make the above changes."
    sudo -v
}

# install all dependency packages for RaSCSI Service
function installPackages() {
    sudo apt-get update && sudo apt-get install git libspdlog-dev libpcap-dev genisoimage python3 python3-venv python3-dev python3-pip nginx libpcap-dev protobuf-compiler bridge-utils libev-dev libevdev2 unar -y </dev/null
}

# cache the pip packages
function cachePipPackages(){
    pushd $WEB_INSTALL_PATH
    # Refresh the sudo authentication, which shouldn't trigger another password prompt
    sudo -v
    sudo pip3 install -r ./requirements.txt
    popd
}

# compile the RaSCSI binaries
function compileRaScsi() {
    cd "$BASE/src/raspberrypi" || exit 1

    echo "Compiling with ${CORES:-1} simultaneous cores..."
    make clean </dev/null

    # Refresh the sudo authentication, which shouldn't trigger another password prompt
    sudo -v
    make -j "${CORES:-1}" all CONNECT_TYPE="${CONNECT_TYPE:-FULLSPEC}" </dev/null
}

function cleanupOutdatedManPage() {
    OUTDATED_MAN_PAGE_DIR=/usr/share/man/man1/
    if [ -f "${OUTDATED_MAN_PAGE_DIR}/$1" ]; then
      sudo rm "${OUTDATED_MAN_PAGE_DIR}/$1"
    fi
}

# install the RaSCSI binaries and modify the service configuration
function installRaScsi() {
    # clean up outdated man pages if they exist
    cleanupOutdatedManPage "rascsi.1"
    cleanupOutdatedManPage "rasctl.1"
    cleanupOutdatedManPage "scsimon.1"
    cleanupOutdatedManPage "rasdump.1"
    cleanupOutdatedManPage "sasidump.1"
    # install
    sudo make install CONNECT_TYPE="${CONNECT_TYPE:-FULLSPEC}" </dev/null
}

function preparePythonCommon() {
    if [ -f "$WEB_INSTALL_PATH/src/rascsi_interface_pb2.py" ]; then
        sudo rm "$WEB_INSTALL_PATH/src/rascsi_interface_pb2.py"
        echo "Deleting old Python protobuf library $WEB_INSTALL_PATH/src/rascsi_interface_pb2.py"
    fi
    if [ -f "$OLED_INSTALL_PATH/src/rascsi_interface_pb2.py" ]; then
        sudo rm "$OLED_INSTALL_PATH/src/rascsi_interface_pb2.py"
        echo "Deleting old Python protobuf library $OLED_INSTALL_PATH/src/rascsi_interface_pb2.py"
    fi
    if [ -f "$PYTHON_COMMON_PATH/src/rascsi_interface_pb2.py" ]; then
        sudo rm "$PYTHON_COMMON_PATH/src/rascsi_interface_pb2.py"
        echo "Deleting old Python protobuf library $PYTHON_COMMON_PATH/src/rascsi_interface_pb2.py"
    fi
    echo "Compiling the Python protobuf library rascsi_interface_pb2.py..."
    protoc -I="$BASE/src/raspberrypi/" --python_out="$PYTHON_COMMON_PATH/src" rascsi_interface.proto
}

# install everything required to run an HTTP server (Nginx + Python Flask App)
function installRaScsiWebInterface() {
    sudo cp -f "$WEB_INSTALL_PATH/service-infra/nginx-default.conf" /etc/nginx/sites-available/default
    sudo cp -f "$WEB_INSTALL_PATH/service-infra/502.html" /var/www/html/502.html

    sudo usermod -a -G $USER www-data

    if [ -f "$SSL_CERTS_PATH/rascsi-web.crt" ]; then
        echo "SSL certificate $SSL_CERTS_PATH/rascsi-web.crt already exists."
    else
        echo "SSL certificate $SSL_CERTS_PATH/rascsi-web.crt does not exist; creating self-signed certificate..."
        sudo mkdir -p "$SSL_CERTS_PATH" || true
        sudo mkdir -p "$SSL_KEYS_PATH" || true
        sudo openssl req -x509 -nodes -sha256 -days 3650 \
            -newkey rsa:4096 \
            -keyout "$SSL_KEYS_PATH/rascsi-web.key" \
            -out "$SSL_CERTS_PATH/rascsi-web.crt" \
            -subj '/CN=rascsi' \
            -addext 'subjectAltName=DNS:rascsi' \
            -addext 'extendedKeyUsage=serverAuth'
    fi

    sudo systemctl reload nginx || true
}

# Creates the dir that RaSCSI uses to store image files
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
    if [ -d "$CFG_PATH" ]; then
        echo "The $CFG_PATH directory already exists."
    else
        echo "The $CFG_PATH directory does not exist; creating..."
        mkdir -p "$CFG_PATH"
        chmod -R 775 "$CFG_PATH"
    fi
}

# Stops the rascsi-web and apache2 processes
function stopOldWebInterface() {
    stopRaScsiWeb

    APACHE_STATUS=$(sudo systemctl status apache2 &> /dev/null; echo $?)
    if [ "$APACHE_STATUS" -eq 0 ] ; then
        echo "Stopping old Apache2 RaSCSI Web..."
        sudo systemctl disable apache2
        sudo systemctl stop apache2
    fi
}

# Checks for upstream changes to the git repo and fast-forwards changes if needed
function updateRaScsiGit() {
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
        echo "There are local changes to the RaSCSI code; we will stash and reapply them."
        git -c user.name="${GIT_COMMITTER_NAME-rascsi}" -c user.email="${GIT_COMMITTER_EMAIL-rascsi@rascsi.com}" stash
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

# Takes a backup copy of the rascsi.service file if it exists
function backupRaScsiService() {
    if [ -f "$SYSTEMD_PATH/rascsi.service" ]; then
        sudo mv "$SYSTEMD_PATH/rascsi.service" "$SYSTEMD_PATH/rascsi.service.old"
        SYSTEMD_BACKUP=true
        echo "Existing version of rascsi.service detected; Backing up to rascsi.service.old"
    else
        SYSTEMD_BACKUP=false
    fi
}

# Offers the choice of enabling token-based authentication for RaSCSI
function configureTokenAuth() {
    echo ""
    echo "Do you want to protect your RaSCSI installation with a password? [y/N]"
    read REPLY

    SECRET_FILE="$HOME/.config/rascsi/rascsi_secret"

    if [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
        echo -n "Enter the password that you want to use: "
        read -r TOKEN
        if [ -f "$HOME/.rascsi_secret" ]; then
            sudo rm "$HOME/.rascsi_secret"
            echo "Removed old RaSCSI token file"
        fi
        if [ -f "$SECRET_FILE" ]; then
            sudo rm "$SECRET_FILE"
            echo "Removed old RaSCSI token file"
        fi
        echo "$TOKEN" > "$SECRET_FILE"

	# Make the secret file owned and only readable by root
        sudo chown root:root "$SECRET_FILE"
        sudo chmod 600 "$SECRET_FILE"
        echo ""
        echo "Configured RaSCSI to use $SECRET_FILE for authentication. This file is readable by root only."
        echo "Make note of your password: you will need it to use rasctl and other RaSCSI clients."
    fi
}

# Modifies and installs the rascsi service
function enableRaScsiService() {
    if [ ! -z "$TOKEN" ]; then
        sudo sed -i "s@^ExecStart.*@& -F $VIRTUAL_DRIVER_PATH -P $SECRET_FILE@" "$SYSTEMD_PATH/rascsi.service"
    else
        sudo sed -i "s@^ExecStart.*@& -F $VIRTUAL_DRIVER_PATH@" "$SYSTEMD_PATH/rascsi.service"
    fi
    echo "Configured rascsi.service to use $VIRTUAL_DRIVER_PATH as default image dir."

    sudo systemctl daemon-reload
    sudo systemctl restart rsyslog
    sudo systemctl enable rascsi # optional - start rascsi at boot
    sudo systemctl start rascsi

}

# Modifies and installs the rascsi-web service
function installWebInterfaceService() {
    echo "Installing the rascsi-web.service configuration..."
    sudo cp -f "$WEB_INSTALL_PATH/service-infra/rascsi-web.service" "$SYSTEMD_PATH/rascsi-web.service"
    sudo sed -i /^ExecStart=/d "$SYSTEMD_PATH/rascsi-web.service"
    echo "$TOKEN"
    if [ ! -z "$TOKEN" ]; then
        sudo sed -i "8 i ExecStart=$WEB_INSTALL_PATH/start.sh --password=$TOKEN" "$SYSTEMD_PATH/rascsi-web.service"
	# Make the service file readable by root only, to protect the token string
        sudo chmod 600 "$SYSTEMD_PATH/rascsi-web.service"
        echo "Granted access to the Web Interface with the token password that you configured for RaSCSI."
    else
        sudo sed -i "8 i ExecStart=$WEB_INSTALL_PATH/start.sh" "$SYSTEMD_PATH/rascsi-web.service"
    fi

    sudo systemctl daemon-reload
    sudo systemctl enable rascsi-web
    sudo systemctl start rascsi-web
}

# Stops the rascsi service if it is running
function stopRaScsi() {
    if [[ -f "$SYSTEMD_PATH/rascsi.service" ]]; then
        SERVICE_RASCSI_RUNNING=0
        sudo systemctl is-active --quiet rascsi.service >/dev/null 2>&1 || SERVICE_RASCSI_RUNNING=$?
        if [[ $SERVICE_RASCSI_RUNNING -eq 0 ]]; then
            sudo systemctl stop rascsi.service
        fi
    fi
}

# Stops the rascsi-web service if it is running
function stopRaScsiWeb() {
    if [[ -f "$SYSTEMD_PATH/rascsi-web.service" ]]; then
        SERVICE_RASCSI_WEB_RUNNING=0
        sudo systemctl is-active --quiet rascsi-web.service >/dev/null 2>&1 || SERVICE_RASCSI_WEB_RUNNING=$?
        if [[ $SERVICE_RASCSI_WEB_RUNNING -eq 0 ]]; then
            sudo systemctl stop rascsi-web.service
        fi
    fi
}

# Stops the rascsi-oled service if it is running
function stopRaScsiScreen() {
    if [[ -f "$SYSTEMD_PATH/monitor_rascsi.service" ]]; then
        SERVICE_MONITOR_RASCSI_RUNNING=0
        sudo systemctl is-active --quiet monitor_rascsi.service >/dev/null 2>&1 || SERVICE_MONITOR_RASCSI_RUNNING=$?
        if [[ $SERVICE_MONITOR_RASCSI_RUNNING -eq 0 ]]; then
          sudo systemctl stop monitor_rascsi.service
        fi
    fi
    if [[ -f "$SYSTEMD_PATH/rascsi-oled.service" ]]; then
        SERVICE_RASCSI_OLED_RUNNING=0
        sudo systemctl is-active --quiet rascsi-oled.service >/dev/null 2>&1 || SERVICE_RASCSI_OLED_RUNNING=$?
        if  [[ $SERVICE_RASCSI_OLED_RUNNING -eq 0 ]]; then
          sudo systemctl stop rascsi-oled.service
        fi
    fi
}

# Stops the rascsi-ctrlboard service if it is running
function stopRaScsiCtrlBoard() {
    if [[ -f "$SYSTEMD_PATH/rascsi-ctrlboard.service" ]]; then
        SERVICE_RASCSI_CTRLBOARD_RUNNING=0
        sudo systemctl is-active --quiet rascsi-ctrlboard.service >/dev/null 2>&1 || SERVICE_RASCSI_CTRLBOARD_RUNNING=$?
        if  [[ SERVICE_RASCSI_CTRLBOARD_RUNNING -eq 0 ]]; then
          sudo systemctl stop rascsi-ctrlboard.service
        fi
    fi
}

# disables and removes the old monitor_rascsi service
function disableOldRaScsiMonitorService() {
    if [ -f "$SYSTEMD_PATH/monitor_rascsi.service" ]; then
        SERVICE_MONITOR_RASCSI_RUNNING=0
        sudo systemctl is-active --quiet monitor_rascsi.service >/dev/null 2>&1 || SERVICE_MONITOR_RASCSI_RUNNING=$?
        if [[ $SERVICE_MONITOR_RASCSI_RUNNING -eq 0 ]]; then
          sudo systemctl stop monitor_rascsi.service
        fi

        SERVICE_MONITOR_RASCSI_ENABLED=0
        sudo systemctl is-enabled --quiet monitor_rascsi.service >/dev/null 2>&1 || SERVICE_MONITOR_RASCSI_ENABLED=$?
        if [[ $SERVICE_MONITOR_RASCSI_ENABLED -eq 0 ]]; then
          sudo systemctl disable monitor_rascsi.service
        fi
        sudo rm $SYSTEMD_PATH/monitor_rascsi.service
    fi
}

# disables the rascsi-oled service
function disableRaScsiOledService() {
    if [ -f "$SYSTEMD_PATH/rascsi-oled.service" ]; then
        SERVICE_RASCSI_OLED_RUNNING=0
        sudo systemctl is-active --quiet rascsi-oled.service >/dev/null 2>&1 || SERVICE_RASCSI_OLED_RUNNING=$?
        if [[ $SERVICE_RASCSI_OLED_RUNNING -eq 0 ]]; then
          sudo systemctl stop rascsi-oled.service
        fi

        SERVICE_RASCSI_OLED_ENABLED=0
        sudo systemctl is-enabled --quiet rascsi-oled.service >/dev/null 2>&1 || SERVICE_RASCSI_OLED_ENABLED=$?
        if [[ $SERVICE_RASCSI_OLED_ENABLED -eq 0 ]]; then
          sudo systemctl disable rascsi-oled.service
        fi
    fi
}

# disables the rascsi-ctrlboard service
function disableRaScsiCtrlBoardService() {
    if [ -f "$SYSTEMD_PATH/rascsi-ctrlboard.service" ]; then
        SERVICE_RASCSI_CTRLBOARD_RUNNING=0
        sudo systemctl is-active --quiet rascsi-ctrlboard.service >/dev/null 2>&1 || SERVICE_RASCSI_CTRLBOARD_RUNNING=$?
        if [[ $SERVICE_RASCSI_CTRLBOARD_RUNNING -eq 0 ]]; then
          sudo systemctl stop rascsi-ctrlboard.service
        fi

        SERVICE_RASCSI_CTRLBOARD_ENABLED=0
        sudo systemctl is-enabled --quiet rascsi-ctrlboard.service >/dev/null 2>&1 || SERVICE_RASCSI_CTRLBOARD_ENABLED=$?
        if [[ $SERVICE_RASCSI_CTRLBOARD_ENABLED -eq 0 ]]; then
          sudo systemctl disable rascsi-ctrlboard.service
        fi
    fi
}

# Stops the macproxy service if it is running
function stopMacproxy() {
    if [ -f "$SYSTEMD_PATH/macproxy.service" ]; then
        sudo systemctl stop macproxy.service
    fi
}

# Checks whether the rascsi-oled service is installed
function isRaScsiScreenInstalled() {
    SERVICE_RASCSI_OLED_ENABLED=0
    if [[ -f "$SYSTEMD_PATH/rascsi-oled.service" ]]; then
        sudo systemctl is-enabled --quiet rascsi-oled.service >/dev/null 2>&1 || SERVICE_RASCSI_OLED_ENABLED=$?
    elif [[ -f "$SYSTEMD_PATH/monitor_rascsi.service" ]]; then
        sudo systemctl is-enabled --quiet monitor_rascsi.service >/dev/null 2>&1 || SERVICE_RASCSI_OLED_ENABLED=$?
    else
        SERVICE_RASCSI_OLED_ENABLED=1
    fi

    echo $SERVICE_RASCSI_OLED_ENABLED
}

# Checks whether the rascsi-ctrlboard service is installed
function isRaScsiCtrlBoardInstalled() {
    SERVICE_RASCSI_CTRLBOARD_ENABLED=0
    if [[ -f "$SYSTEMD_PATH/rascsi-ctrlboard.service" ]]; then
        sudo systemctl is-enabled --quiet rascsi-ctrlboard.service >/dev/null 2>&1 || SERVICE_RASCSI_CTRLBOARD_ENABLED=$?
    else
        SERVICE_RASCSI_CTRLBOARD_ENABLED=1
    fi

    echo $SERVICE_RASCSI_CTRLBOARD_ENABLED
}

# Checks whether the rascsi-oled service is running
function isRaScsiScreenRunning() {
    SERVICE_RASCSI_OLED_RUNNING=0
    if [[ -f "$SYSTEMD_PATH/rascsi-oled.service" ]]; then
        sudo systemctl is-active --quiet rascsi-oled.service >/dev/null 2>&1 || SERVICE_RASCSI_OLED_RUNNING=$?
    elif [[ -f "$SYSTEMD_PATH/monitor_rascsi.service" ]]; then
        sudo systemctl is-active --quiet monitor_rascsi.service >/dev/null 2>&1 || SERVICE_RASCSI_OLED_RUNNING=$?
    else
        SERVICE_RASCSI_OLED_RUNNING=1
    fi

    echo $SERVICE_RASCSI_OLED_RUNNING
}

# Checks whether the rascsi-oled service is running
function isRaScsiCtrlBoardRunning() {
    SERVICE_RASCSI_CTRLBOARD_RUNNING=0
    if [[ -f "$SYSTEMD_PATH/rascsi-ctrlboard.service" ]]; then
        sudo systemctl is-active --quiet rascsi-ctrlboard.service >/dev/null 2>&1 || SERVICE_RASCSI_CTRLBOARD_RUNNING=$?
    else
        SERVICE_RASCSI_CTRLBOARD_RUNNING=1
    fi

    echo $SERVICE_RASCSI_CTRLBOARD_RUNNING
}


# Starts the rascsi-oled service if installed
function startRaScsiScreen() {
    if [[ $(isRaScsiScreenInstalled) -eq 0 ]] && [[ $(isRaScsiScreenRunning) -ne 1 ]]; then
        sudo systemctl start rascsi-oled.service
        showRaScsiScreenStatus
    fi
}

# Starts the rascsi-ctrlboard service if installed
function startRaScsiCtrlBoard() {
    if [[ $(isRaScsiCtrlBoardInstalled) -eq 0 ]] && [[ $(isRaScsiCtrlBoardRunning) -ne 1 ]]; then
        sudo systemctl start rascsi-ctrlboard.service
        showRaScsiCtrlBoardStatus
    fi
}

# Starts the macproxy service if installed
function startMacproxy() {
    if [ -f "$SYSTEMD_PATH/macproxy.service" ]; then
        sudo systemctl start macproxy.service
        showMacproxyStatus
    fi
}

# Shows status for the rascsi service
function showRaScsiStatus() {
    systemctl status rascsi | tee
}

# Shows status for the rascsi-web service
function showRaScsiWebStatus() {
    systemctl status rascsi-web | tee
}

# Shows status for the rascsi-oled service
function showRaScsiScreenStatus() {
    systemctl status rascsi-oled | tee
}

# Shows status for the rascsi-ctrlboard service
function showRaScsiCtrlBoardStatus() {
    systemctl status rascsi-ctrlboard | tee
}

# Shows status for the macproxy service
function showMacproxyStatus() {
    systemctl status macproxy | tee
}

# Creates a drive image file with specific parameters
function createDrive600MB() {
    createDrive 600 "HD600"
}

# Creates a drive image file and prompts for parameters
function createDriveCustom() {
    driveSize=-1
    until [ $driveSize -ge "10" ] && [ $driveSize -le "4000" ]; do
        echo "What drive size would you like (in MB) (10-4000)"
        read driveSize

        echo "How would you like to name that drive?"
        read driveName
    done

    createDrive "$driveSize" "$driveName"
}

# Creates an HFS file system
function formatDrive() {
    diskPath="$1"
    volumeName="$2"

    if [ ! -x $HFS_FORMAT ]; then
        # Install hfsutils to have hformat to format HFS
        sudo apt-get install hfsutils --assume-yes </dev/null
    fi

    if [ ! -x $HFDISK_BIN ]; then
        # Clone, compile and install 'hfdisk', partition tool
        git clone git://www.codesrc.com/git/hfdisk.git
        cd hfdisk || exit 1
        make

        sudo cp hfdisk /usr/bin/hfdisk
    fi

    # Inject hfdisk commands to create Drive with correct partitions
    # https://www.codesrc.com/mediawiki/index.php/HFSFromScratch
    # i                         initialize partition map
    # continue with default first block
    # C                         Create 1st partition with type specified next) 
    # continue with default
    # 32                        32 blocks (required for HFS+)
    # Driver_Partition          Partition Name
    # Apple_Driver              Partition Type  (available types: Apple_Driver, Apple_Driver43, Apple_Free, Apple_HFS...)
    # C                         Create 2nd partition with type specified next
    # continue with default first block
    # continue with default block size (rest of the disk)
    # ${volumeName}             Partition name provided by user
    # Apple_HFS                 Partition Type
    # w                         Write partition map to disk
    # y                         Confirm partition table
    # p                         Print partition map
    (echo i; echo ; echo C; echo ; echo 32; echo "Driver_Partition"; echo "Apple_Driver"; echo C; echo ; echo ; echo "${volumeName}"; echo "Apple_HFS"; echo w; echo y; echo p;) | $HFDISK_BIN "$diskPath"
    partitionOk=$?

    if [ $partitionOk -eq 0 ]; then
        if [ ! -f "$LIDO_DRIVER" ];then
            echo "Lido driver couldn't be found. Make sure RaSCSI is up-to-date with git pull"
            return 1
        fi

        # Burn Lido driver to the disk
        dd if="$LIDO_DRIVER" of="$diskPath" seek=64 count=32 bs=512 conv=notrunc

        driverInstalled=$?
        if [ $driverInstalled -eq 0 ]; then
            # Format the partition with HFS file system
            $HFS_FORMAT -l "${volumeName}" "$diskPath" 1
            hfsFormattedOk=$?
            if [ $hfsFormattedOk -eq 0 ]; then
                echo "Disk created with success."
            else
                echo "Unable to format HFS partition."
                return 4
            fi
        else
            echo "Unable to install Lido Driver."
            return 3
        fi
    else
        echo "Unable to create the partition."
        return 2
    fi
}

# Creates an image file
function createDrive() {
    if [ $# -ne 2 ]; then
        echo "To create a Drive, volume size and volume name must be provided"
        echo "$ createDrive 600 \"RaSCSI Drive\""
        echo "Drive wasn't created."
        return
    fi

    driveSize=$1
    driveName=$2
    mkdir -p "$VIRTUAL_DRIVER_PATH"
    drivePath="${VIRTUAL_DRIVER_PATH}/${driveSize}MB.hda"

    if [ ! -f "$drivePath" ]; then
        echo "Creating a ${driveSize}MB Drive"
        truncate --size "${driveSize}m" "$drivePath"

        echo "Formatting drive with HFS"
        formatDrive "$drivePath" "$driveName"

    else
        echo "Error: drive already exists"
    fi
}

# Modifies system configurations for a wired network bridge
function setupWiredNetworking() {
    echo "Setting up wired network..."

    LAN_INTERFACE=eth0

    echo "$LAN_INTERFACE will be configured for network forwarding with DHCP."
    echo ""
    echo "WARNING: If you continue, the IP address of your Pi may change upon reboot."
    echo "Please make sure you will not lose access to the Pi system."
    echo ""
    echo "Do you want to proceed with network configuration using the default settings? [Y/n]"
    read REPLY

    if [ "$REPLY" == "N" ] || [ "$REPLY" == "n" ]; then
        echo "Available wired interfaces on this system:"
        echo `ip -o addr show scope link | awk '{split($0, a); print $2}' | grep eth`
        echo "Please type the wired interface you want to use and press Enter:"
        read SELECTED
        LAN_INTERFACE=$SELECTED
    fi

    if [ "$(grep -c "^denyinterfaces" /etc/dhcpcd.conf)" -ge 1 ]; then
        echo "WARNING: Network forwarding may already have been configured. Proceeding will overwrite the configuration."
        echo "Press enter to continue or CTRL-C to exit"
        read REPLY
        sudo sed -i /^denyinterfaces/d /etc/dhcpcd.conf
    fi
    sudo bash -c 'echo "denyinterfaces '$LAN_INTERFACE'" >> /etc/dhcpcd.conf'
    echo "Modified /etc/dhcpcd.conf"

    # default config file is made for eth0, this will set the right net interface
    sudo bash -c 'sed s/eth0/'"$LAN_INTERFACE"'/g '"$BASE"'/src/raspberrypi/os_integration/rascsi_bridge > /etc/network/interfaces.d/rascsi_bridge'
    echo "Modified /etc/network/interfaces.d/rascsi_bridge"

    echo "Configuration completed!"
    echo "Please make sure you attach a DaynaPORT network adapter to your RaSCSI configuration."
    echo "Either use the Web UI, or do this on the command line (assuming SCSI ID 6):"
    echo "rasctl -i 6 -c attach -t scdp -f $LAN_INTERFACE"
    echo ""
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
    WLAN_INTERFACE="wlan0"

    echo "$WLAN_INTERFACE will be configured for network forwarding with static IP assignment."
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
        echo `ip -o addr show scope link | awk '{split($0, a); print $2}' | grep wlan`
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
        sudo apt-get install iptables --assume-yes </dev/null
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
        sudo apt-get install iptables-persistent --assume-yes </dev/null
    fi
    echo "Modified /etc/iptables/rules.v4"

    echo "Configuration completed!"
    echo ""
    echo "Please make sure you attach a DaynaPORT network adapter to your RaSCSI configuration"
    echo "Either use the Web UI, or do this on the command line (assuming SCSI ID 6):"
    echo "rasctl -i 6 -c attach -t scdp -f $WLAN_INTERFACE:$ROUTER_IP/$CIDR"
    echo ""
    echo "We need to reboot your Pi"
    echo "Press Enter to reboot or CTRL-C to exit"
    read REPLY

    echo "Rebooting..."
    sleep 3
    sudo reboot
}

# Downloads, compiles, and installs Netatalk (AppleShare server)
function installNetatalk() {
    NETATALK_VERSION="2-220702"
    AFP_SHARE_PATH="$HOME/afpshare"
    AFP_SHARE_NAME="Pi File Server"

    echo "Downloading netatalk-$NETATALK_VERSION to $HOME"
    cd $HOME || exit 1
    wget -O "netatalk-$NETATALK_VERSION.tar.gz" "https://github.com/rdmark/Netatalk-2.x/archive/refs/tags/netatalk-$NETATALK_VERSION.tar.gz" </dev/null
    tar -xzvf netatalk-$NETATALK_VERSION.tar.gz

    cd "$HOME/Netatalk-2.x-netatalk-$NETATALK_VERSION/contrib/shell_utils" || exit 1
    ./debian_install.sh -j="${CORES:-1}" -n="$AFP_SHARE_NAME" -p="$AFP_SHARE_PATH" || exit 1
}

# Downloads, compiles, and installs Macproxy (web proxy)
function installMacproxy {
    PORT=5000

    echo "Macproxy is a Web Proxy for all vintage Web Browsers -- not only for Macs!"
    echo ""
    echo "By default, Macproxy listens to port $PORT, but you can choose any other available port."
    echo -n "Enter a port number 1024 - 65535, or press Enter to use the default port: "

    read -r CHOICE
    if [ $CHOICE -ge "1024" ] && [ $CHOICE -le "65535" ]; then
        PORT=$CHOICE
    else
        echo "Using the default port $PORT"
    fi

    ( sudo apt-get update && sudo apt-get install python3 python3-venv --assume-yes ) </dev/null

    MACPROXY_VER="21.12.3"
    MACPROXY_PATH="$HOME/macproxy-$MACPROXY_VER"
    if [ -d "$MACPROXY_PATH" ]; then
        echo "The $MACPROXY_PATH directory already exists. Delete it to proceed with the installation."
        exit 1
    fi
    cd "$HOME" || exit 1
    wget -O "macproxy-$MACPROXY_VER.tar.gz" "https://github.com/rdmark/macproxy/archive/refs/tags/$MACPROXY_VER.tar.gz" </dev/null
    tar -xzvf "macproxy-$MACPROXY_VER.tar.gz"

    stopMacproxy
    sudo cp "$MACPROXY_PATH/macproxy.service" "$SYSTEMD_PATH"
    sudo sed -i /^ExecStart=/d "$SYSTEMD_PATH/macproxy.service"
    sudo sed -i "8 i ExecStart=$MACPROXY_PATH/start_macproxy.sh" "$SYSTEMD_PATH/macproxy.service"
    sudo systemctl daemon-reload
    sudo systemctl enable macproxy
    startMacproxy

    echo -n "Macproxy is now running on IP "
    echo -n `ip -4 addr show scope global | grep -oP '(?<=inet\s)\d+(\.\d+){3}'`
    echo " port $PORT"
    echo "Configure your browser to use the above as http (and https) proxy."
    echo ""
}

# updates configuration files and installs packages needed for the OLED screen script
function installRaScsiScreen() {
    echo "IMPORTANT: This configuration requires a OLED screen to be installed onto your RaSCSI board."
    echo "See wiki for more information: https://github.com/akuker/RASCSI/wiki/OLED-Status-Display-(Optional)"
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

    if [ -z "$TOKEN" ]; then
        echo ""
        echo "Did you protect your RaSCSI installation with a token password? [y/N]"
        read -r REPLY
        if [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
            echo -n "Enter the password that you configured with RaSCSI at the time of installation: "
            read -r TOKEN
        fi
    fi

    stopRaScsiScreen
    disableRaScsiCtrlBoardService
    updateRaScsiGit

    sudo apt-get update && sudo apt-get install libjpeg-dev libpng-dev libopenjp2-7-dev i2c-tools raspi-config -y </dev/null

    if [[ $(grep -c "^dtparam=i2c_arm=on" /boot/config.txt) -ge 1 ]]; then
        echo "NOTE: I2C support seems to have been configured already."
        REBOOT=0
    else
        sudo raspi-config nonint do_i2c 0 </dev/null
        echo "Modified the Raspberry Pi boot configuration to enable I2C."
        echo "A reboot will be required for the change to take effect."
        REBOOT=1
    fi

    echo "Installing the rascsi-oled.service configuration..."
    sudo cp -f "$OLED_INSTALL_PATH/service-infra/rascsi-oled.service" "$SYSTEMD_PATH/rascsi-oled.service"
    sudo sed -i /^ExecStart=/d "$SYSTEMD_PATH/rascsi-oled.service"
    if [ ! -z "$TOKEN" ]; then
        sudo sed -i "8 i ExecStart=$OLED_INSTALL_PATH/start.sh --rotation=$ROTATION --height=$SCREEN_HEIGHT --password=$TOKEN" "$SYSTEMD_PATH/rascsi-oled.service"
	# Make the service file readable by root only, to protect the token string
        sudo chmod 600 "$SYSTEMD_PATH/rascsi-oled.service"
        echo "Granted access to the OLED Monitor with the password that you configured for RaSCSI."
    else
        sudo sed -i "8 i ExecStart=$OLED_INSTALL_PATH/start.sh --rotation=$ROTATION --height=$SCREEN_HEIGHT" "$SYSTEMD_PATH/rascsi-oled.service"
    fi

    sudo systemctl daemon-reload

    # ensure that the old monitor_rascsi service is disabled and removed before the new one is installed
    disableOldRaScsiMonitorService

    sudo systemctl daemon-reload
    sudo systemctl enable rascsi-oled

    if [ $REBOOT -eq 1 ]; then
        echo ""
        echo "The rascsi-oled service will start on the next Pi boot."
        echo "Press Enter to reboot or CTRL-C to exit"
        read

        echo "Rebooting..."
        sleep 3
        sudo reboot
    fi

    sudo systemctl start rascsi-oled
}

# updates configuration files and installs packages needed for the CtrlBoard script
function installRaScsiCtrlBoard() {
    echo "IMPORTANT: This configuration requires a RaSCSI Control Board connected to your RaSCSI board."
    echo "See wiki for more information: https://github.com/akuker/RASCSI/wiki/RaSCSI-Control-Board"
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

    if [ -z "$TOKEN" ]; then
        echo ""
        echo "Did you protect your RaSCSI installation with a token password? [y/N]"
        read -r REPLY
        if [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
            echo -n "Enter the password that you configured with RaSCSI at the time of installation: "
            read -r TOKEN
        fi
    fi

    stopRaScsiCtrlBoard
    updateRaScsiGit

    sudo apt-get update && sudo apt-get install libjpeg-dev libpng-dev libopenjp2-7-dev i2c-tools raspi-config -y </dev/null
    # install python packages through apt that need compilation
    sudo apt-get install python3-cbor2 -y </dev/null

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

    echo "Installing the rascsi-ctrlboard.service configuration..."
    sudo cp -f "$CTRLBOARD_INSTALL_PATH/service-infra/rascsi-ctrlboard.service" "$SYSTEMD_PATH/rascsi-ctrlboard.service"
    sudo sed -i /^ExecStart=/d "$SYSTEMD_PATH/rascsi-ctrlboard.service"
    if [ ! -z "$TOKEN" ]; then
        sudo sed -i "8 i ExecStart=$CTRLBOARD_INSTALL_PATH/start.sh --rotation=$ROTATION --password=$TOKEN" "$SYSTEMD_PATH/rascsi-ctrlboard.service"
        sudo chmod 600 "$SYSTEMD_PATH/rascsi-ctrlboard.service"
        echo "Granted access to the RaSCSI Control Board UI with the password that you configured for RaSCSI."
    else
        sudo sed -i "8 i ExecStart=$CTRLBOARD_INSTALL_PATH/start.sh --rotation=$ROTATION" "$SYSTEMD_PATH/rascsi-ctrlboard.service"
    fi

    sudo systemctl daemon-reload

    # ensure that the old monitor_rascsi or rascsi-oled service is disabled and removed before the new one is installed
    disableOldRaScsiMonitorService
    disableRaScsiOledService

    sudo systemctl daemon-reload
    sudo systemctl enable rascsi-ctrlboard

    if [ $REBOOT -eq 1 ]; then
        echo ""
        echo "The rascsi-ctrlboard service will start on the next Pi boot."
        echo "Press Enter to reboot or CTRL-C to exit"
        read

        echo "Rebooting..."
        sleep 3
        sudo reboot
    fi

    sudo systemctl start rascsi-ctrlboard
}

# Prints a notification if the rascsi.service file was backed up
function notifyBackup {
    if "$SYSTEMD_BACKUP"; then
        echo ""
        echo "IMPORTANT: $SYSTEMD_PATH/rascsi.service has been overwritten."
        echo "A backup copy was saved as rascsi.service.old in the same directory."
        echo "Please inspect the backup file and restore configurations that are important to your setup."
        echo ""
    fi
}

# Creates the group and modifies current user for Web Interface auth
function enableWebInterfaceAuth {
    AUTH_GROUP="rascsi"

    if [ $(getent group "$AUTH_GROUP") ]; then
        echo "The '$AUTH_GROUP' group already exists."
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
              echo "Installing / Updating RaSCSI Service (${CONNECT_TYPE:-FULLSPEC}) + Web Interface"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              echo "- Modify and enable Apache2 and Nginx web services"
              echo "- Create files and directories"
              echo "- Change permissions of files and directories"
              echo "- Modify user groups and permissions"
              echo "- Install binaries to /usr/local/bin"
              echo "- Install manpages to /usr/local/man"
              echo "- Create a self-signed certificate in /etc/ssl"
              sudoCheck
              configureTokenAuth
              stopOldWebInterface
              updateRaScsiGit
              createImagesDir
              createCfgDir
              installPackages
              stopRaScsiScreen
              stopRaScsi
              compileRaScsi
              backupRaScsiService
              installRaScsi
              enableRaScsiService
              preparePythonCommon
              if [[ $(isRaScsiScreenInstalled) -eq 0 ]]; then
                  echo "Detected rascsi oled service; will run the installation steps for the OLED monitor."
                  installRaScsiScreen
              elif [[ $(isRaScsiCtrlBoardInstalled) -eq 0 ]]; then
                  echo "Detected rascsi control board service; will run the installation steps for the control board ui."
                  installRaScsiCtrlBoard
              fi
              cachePipPackages
              installRaScsiWebInterface
              installWebInterfaceService
              enableWebInterfaceAuth
              showRaScsiScreenStatus
              showRaScsiCtrlBoardStatus
              showRaScsiStatus
              showRaScsiWebStatus
              notifyBackup
              echo "Installing / Updating RaSCSI Service (${CONNECT_TYPE:-FULLSPEC}) + Web Interface - Complete!"
          ;;
          2)
              echo "Installing / Updating RaSCSI Service (${CONNECT_TYPE:-FULLSPEC})"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              echo "- Create files ans directories"
              echo "- Change permissions of files and directories"
              echo "- Modify user groups and permissions"
              echo "- Install binaries to /usr/local/bin"
              echo "- Install manpages to /usr/local/man"
              sudoCheck
              configureTokenAuth
              updateRaScsiGit
              createImagesDir
              installPackages
              stopRaScsiScreen
              stopRaScsi
              compileRaScsi
              backupRaScsiService
              preparePythonCommon
              installRaScsi
              enableRaScsiService
              if [[ $(isRaScsiScreenInstalled) -eq 0 ]]; then
                  echo "Detected rascsi oled service; will run the installation steps for the OLED monitor."
                  installRaScsiScreen
              elif [[ $(isRaScsiCtrlBoardInstalled) -eq 0 ]]; then
                  echo "Detected rascsi control board service; will run the installation steps for the control board ui."
                  installRaScsiCtrlBoard
              fi
              showRaScsiScreenStatus
              showRaScsiCtrlBoardStatus
              showRaScsiStatus
              notifyBackup
              echo "Installing / Updating RaSCSI Service (${CONNECT_TYPE:-FULLSPEC}) - Complete!"
          ;;
          3)
              echo "Installing / Updating RaSCSI OLED Screen"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              echo "- Modify the Raspberry Pi boot configuration (may require a reboot)"
              sudoCheck
              preparePythonCommon
              installRaScsiScreen
              showRaScsiScreenStatus
              echo "Installing / Updating RaSCSI OLED Screen - Complete!"
          ;;
          4)
              echo "Creating an HFS formatted 600MB drive image with LIDO driver"
              createDrive600MB
              echo "Creating an HFS formatted 600MB drive image with LIDO driver - Complete!"
          ;;
          5)
              echo "Creating an HFS formatted drive image with LIDO driver"
              createDriveCustom
              echo "Creating an HFS formatted drive image with LIDO driver - Complete!"
          ;;
          6)
              echo "Configuring wired network bridge"
              echo "This script will make the following changes to your system:"
              echo "- Create a virtual network bridge interface in /etc/network/interfaces.d"
              echo "- Modify /etc/dhcpcd.conf to bridge the Ethernet interface (may change the IP address; requires a reboot)"
              sudoCheck
              showMacNetworkWired
              setupWiredNetworking
              echo "Configuring wired network bridge - Complete!"
          ;;
          7)
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
          8)
              installNetatalk
              echo "Installing AppleShare File Server - Complete!"
          ;;
          9)
              echo "Installing Web Proxy Server"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              sudoCheck
              installMacproxy
              echo "Installing Web Proxy Server - Complete!"
          ;;
          10)
              echo "Configuring RaSCSI stand-alone (${CONNECT_TYPE:-FULLSPEC})"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Create directories and change permissions"
              echo "- Install binaries to /usr/local/bin"
              echo "- Install manpages to /usr/local/man"
              sudoCheck
              updateRaScsiGit
              createImagesDir
              installPackages
              stopRaScsi
              compileRaScsi
              installRaScsi
              echo "Configuring RaSCSI stand-alone (${CONNECT_TYPE:-FULLSPEC}) - Complete!"
              echo "Use 'rascsi' to launch RaSCSI, and 'rasctl' to control the running process."
          ;;
          11)
              echo "Configuring RaSCSI Web Interface stand-alone"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              echo "- Modify and enable Apache2 and Nginx web service"
              echo "- Create directories and change permissions"
              echo "- Modify user groups and permissions"
              echo "- Create a self-signed certificate in /etc/ssl"
              sudoCheck
              updateRaScsiGit
              createCfgDir
              installPackages
              preparePythonCommon
              cachePipPackages
              installRaScsiWebInterface
              echo "Configuring RaSCSI Web Interface stand-alone - Complete!"
              echo "Launch the Web Interface with the 'start.sh' script. To use a custom port for the web server: 'start.sh --web-port=8081"
          ;;
          12)
              echo "Installing / Updating RaSCSI Control Board UI"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              echo "- Stop and disable the RaSCSI OLED service if it is running"
              echo "- Modify the Raspberry Pi boot configuration (may require a reboot)"
              sudoCheck
              preparePythonCommon
              installRaScsiCtrlBoard
              showRaScsiCtrlBoardStatus
              echo "Installing / Updating RaSCSI Control Board UI - Complete!"
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

   until [ $choice -ge "0" ] && [ $choice -le "12" ]; do
       echo -n "Enter your choice (0-12) or CTRL-C to exit: "
       read -r choice
   done

   runChoice "$choice"
}

# Shows the interactive main menu of the script
function showMenu() {
    echo ""
    echo "Choose among the following options:"
    echo "INSTALL/UPDATE RASCSI (${CONNECT_TYPE-FULLSPEC} version)"
    echo "  1) install or update RaSCSI Service + Web Interface"
    echo "  2) install or update RaSCSI Service"
    echo "  3) install or update RaSCSI OLED Screen (requires hardware)"
    echo "CREATE HFS FORMATTED (MAC) IMAGE WITH LIDO DRIVERS"
    echo "** For the Mac Plus, it's better to create an image through the Web Interface **"
    echo "  4) 600MB drive (suggested size)"
    echo "  5) custom drive size (up to 4000MB)"
    echo "NETWORK BRIDGE ASSISTANT"
    echo "  6) configure network bridge for Ethernet (DHCP)"
    echo "  7) configure network bridge for WiFi (static IP + NAT)" 
    echo "INSTALL COMPANION APPS"
    echo "  8) install AppleShare File Server (Netatalk)"
    echo "  9) install Web Proxy Server (Macproxy)"
    echo "ADVANCED OPTIONS"
    echo " 10) compile and install RaSCSI stand-alone"
    echo " 11) configure the RaSCSI Web Interface stand-alone"
    echo "EXPERIMENTAL FEATURES"
    echo " 12) install or update RaSCSI Control Board UI (requires hardware)"
}

# parse arguments passed to the script
while [ "$1" != "" ]; do
    PARAM=$(echo "$1" | awk -F= '{print $1}')
    VALUE=$(echo "$1" | awk -F= '{print $2}')
    case $PARAM in
        -c | --connect_type)
            CONNECT_TYPE=$VALUE
            ;;
        -r | --run_choice)
            RUN_CHOICE=$VALUE
            ;;
        -j | --cores)
            CORES=$VALUE
            ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            exit 1
            ;;
    esac
    case $VALUE in
        FULLSPEC | STANDARD | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12)
            ;;
        *)
            echo "ERROR: unknown option \"$VALUE\""
            exit 1
            ;;
    esac
    shift
done

showRaSCSILogo
initialChecks

if [ -z "${RUN_CHOICE}" ]; then # RUN_CHOICE is unset, show menu
    showMenu
    readChoice
else
    runChoice "$RUN_CHOICE"
fi
