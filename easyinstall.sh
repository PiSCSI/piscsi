#!/usr/bin/env bash

# BSD 3-Clause License
# Author @sonique6784
# Copyright (c) 2020, sonique6784
# Copyright (c) 2021-2026, Daniel Markstedt <daniel@mindani.net>

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

USER=$(whoami)
BASE=$(dirname "$(readlink -f "${0}")")
PISCSI_STATEDIR="/var/lib/piscsi"
VIRTUAL_DRIVER_PATH="$PISCSI_STATEDIR/images"
CFG_PATH="$PISCSI_STATEDIR/config"
DRIVER_PATH="$PISCSI_STATEDIR/data/mac-hard-disk-drivers"
SYSTEMD_PATH="/usr/lib/systemd/system"
HFDISK_BIN=/usr/bin/hfdisk
TOKEN=""
SECRET_FILE="$CFG_PATH/secret"
FILE_SHARE_PATH="$PISCSI_STATEDIR/shared"
FILE_SHARE_NAME="Pi File Server"

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
    if [[ $HEADLESS ]]; then
        echo "Skipping password check in headless mode"
        return 0
    fi
    echo "Input your password to allow this script to make the above changes."
    sudo -v
}

# update apt repositories
function updateAptSources() {
    if [[ $SKIP_PACKAGES ]]; then
        echo "Skipping package update"
        return 0
    fi
    sudo apt-get update
}

# Offers the choice of enabling token-based authentication for PiSCSI, or disables it if enabled
function configureTokenAuth() {
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

    printf '%s\n' "$TOKEN" | sudo tee "$SECRET_FILE" > /dev/null

    # Make the secret file owned and only readable by root
    sudo chown root:root "$SECRET_FILE"
    sudo chmod 600 "$SECRET_FILE"
    sudo sed -i "s@^ExecStart.*@& -P $SECRET_FILE@" "$SYSTEMD_PATH/piscsi.service"
    sudo systemctl daemon-reload
    sudo systemctl start piscsi.service

    echo ""
    echo "Configured PiSCSI to use $SECRET_FILE for authentication. This file is readable by root only."
    echo "Make note of your password: you will need it to use scsictl and other PiSCSI clients."
    echo "If you have PiSCSI clients installed, please re-run the installation scripts, or update the systemd config manually."
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

# Starts the macproxy service if installed
function startMacproxy() {
    if [ -f "$SYSTEMD_PATH/macproxy.service" ]; then
        sudo systemctl start macproxy.service
        showServiceStatus "macproxy"
    fi
}

# Shows status for a service
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

# Clone, compile and install 'hfsutils', HFS disk image tools
function installHfsutils() {
    if ! sudo apt-get install --no-install-recommends --assume-yes -qq hfsutils; then
        echo "hfsutils package not found in apt repositories; compiling from source..."
    else
        return 0
    fi

    sudo apt-get install --no-install-recommends --assume-yes -qq autoconf automake libtool m4 </dev/null

    if [ -d "$BASE/hfsutils" ]; then
        echo "hfsutils source dir already exists; deleting before re-cloning..."
        rm -rf "$BASE/hfsutils"
    fi

    git clone https://github.com/rdmark/hfsutils.git "$BASE/hfsutils"
    cd "$BASE/hfsutils" || exit 1
    git checkout v2025.12.1
    autoreconf -i
    ./configure
    make
    sudo make install
}

# Fetch HFS drivers that the Web Interface uses
function fetchHardDiskDrivers() {
    cd "$BASE" || exit 1
    sudo wget -N "https://github.com/ingpaschke/miniscsi/releases/download/v1.00/driver.bin" -O "$DRIVER_PATH/miniscsi-1.0.bin"
    wget -N "https://www.dropbox.com/s/gcs4v5pcmk7rxtb/mac-hard-disk-drivers.zip?dl=1" -O mac-hard-disk-drivers.zip
    sudo unzip -j mac-hard-disk-drivers.zip -d "$DRIVER_PATH"
    rm mac-hard-disk-drivers.zip
}

# Installs and configures Netatalk (AFP server)
function installNetatalk() {
    local NETATALK_CONFIG_PATH="/etc/netatalk"
    local AFPCONF
    local ADDITIONAL_SHARE_NAME
    local ADDITIONAL_SHARE_PATH
    local APPLETALK_INTERFACE

    if [ -d "$NETATALK_CONFIG_PATH" ]; then
        echo
        echo "WARNING: Netatalk configuration dir $NETATALK_CONFIG_PATH already exists."
        echo "This installation process will overwrite existing binaries and configurations."
        echo "No shared files will be deleted, but you may have to manually restore your settings after the installation."
        echo
        echo "Do you want to proceed with the installation? [y/N]"
        read -r REPLY
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 0
        fi
    fi

    echo "Do you want to share the $VIRTUAL_DRIVER_PATH dir as a Netatalk volume? [y/N]"
    read -r REPLY
    if [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
        ADDITIONAL_SHARE_PATH="$VIRTUAL_DRIVER_PATH"
        ADDITIONAL_SHARE_NAME="PiSCSI Images"
    fi

    if [ -f "/etc/network/interfaces.d/piscsi_bridge" ]; then
        echo "PiSCSI network bridge detected. Using 'piscsi_bridge' interface for AppleTalk."
        APPLETALK_INTERFACE="piscsi_bridge"
    fi

    if [[ $SKIP_PACKAGES ]]; then
        echo "Skipping package installation"
    else
        sudo apt-get update || true
        sudo apt-get install atalkd cups netatalk papd timelord --assume-yes --no-install-recommends </dev/null
    fi

    echo
    echo "Modifying service configurations..."

    AFPCONF="$NETATALK_CONFIG_PATH/afp.conf"

    if [[ $(lsmod | grep -c appletalk) -eq 0 ]]; then
        echo
        echo "Your system may not have support for AppleTalk networking."
        echo "You can still use Netatalk with Macs that support AppleTalk over TCP/IP (DSI)."
        echo "In the Chooser, input the IP address of the network interface that is connected to the rest of your network:"
        ip -4 addr show scope global | grep -oP '(?<=inet\s)\d+(\.\d+){3}'
    else
        echo
        echo "Enabling support for AppleTalk networking."
        if ! grep -q '^[[:space:]]*appletalk[[:space:]]*=[[:space:]]*yes[[:space:]]*$' "$AFPCONF"; then
            sudo sed -i '/^\[Global\]/a appletalk = yes' "$AFPCONF"
        fi
    fi

    if ! grep -q '^uam list' "$AFPCONF"; then
        sudo sed -i '/^\[Global\]/a uam list = uams_clrtxt.so uams_guest.so uams_dhx.so uams_dhx2.so' "$AFPCONF"
        echo "Added 'uam list' to [Global] in afp.conf"
    else
        echo "'uam list' already exists; not updating afp.conf"
    fi

    addNetatalkShare "$FILE_SHARE_NAME" "$FILE_SHARE_PATH"
    if [[ $ADDITIONAL_SHARE_NAME && $ADDITIONAL_SHARE_PATH ]]; then
        addNetatalkShare "$ADDITIONAL_SHARE_NAME" "$ADDITIONAL_SHARE_PATH"
    fi

    if [[ $APPLETALK_INTERFACE ]]; then
        echo "$NETATALK_CONFIG_PATH/atalkd.conf:"
        echo "$APPLETALK_INTERFACE" | sudo tee -a "$NETATALK_CONFIG_PATH/atalkd.conf"
    fi

    echo "$NETATALK_CONFIG_PATH/papd.conf:"
    echo "cupsautoadd:op=root:" | sudo tee -a "$NETATALK_CONFIG_PATH/papd.conf"
    sudo usermod -a -G lpadmin "$USER"
    sudo cupsctl --remote-admin WebInterface=yes
    if [[ $(sudo grep -c 'PreserveJobHistory' /etc/cups/cupsd.conf) -eq 0 ]]; then
        echo "/etc/cups/cupsd.conf:"
        sudo sed -i '/MaxLogSize/a PreserveJobHistory\\ No' /etc/cups/cupsd.conf
    fi

    echo
    echo "Netatalk daemons are now installed and running, and should be discoverable by your Macs."
    echo "To authenticate with the file server, use the current username ($USER) and password."
    echo
    echo "To learn more about Netatalk and its capabilities, visit https://netatalk.io"
    echo "Enjoy AFP file sharing!"
}

# Adds a named AFP share unless it is already configured
function addNetatalkShare() {
    local share_name="$1"
    local share_path="$2"
    local afp_conf="$NETATALK_CONFIG_PATH/afp.conf"

    if grep -Fqx "[$share_name]" "$afp_conf"; then
        echo "Share section [$share_name] already exists; not updating afp.conf"
        return
    fi

    {
        echo
        echo "[$share_name]"
        echo "path = $share_path"
        echo "volume name = $share_name"
    } | sudo tee -a "$afp_conf" > /dev/null
    echo "Added share section for $share_name to afp.conf"
}

# Downloads, compiles, and installs Macproxy (web proxy)
function installMacproxy {
    PORT=5001

    echo "Macproxy Classic is a Web Proxy for all vintage Web Browsers -- not only for Macs!"
    echo ""
    echo "By default, Macproxy Classic listens to port $PORT, but you can choose any other available port."
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
        sudo apt-get install python3 python3-venv --assume-yes --no-install-recommends </dev/null
    fi

    MACPROXY_VER="25.11.1"
    MACPROXY_PATH="$HOME/macproxy_classic-$MACPROXY_VER"
    if [ -d "$MACPROXY_PATH" ]; then
        echo "The $MACPROXY_PATH directory already exists. Deleting before downloading again..."
        sudo rm -rf "$MACPROXY_PATH"
    fi
    cd "$HOME" || exit 1
    wget -O "macproxy_classic-$MACPROXY_VER.tar.gz" "https://github.com/rdmark/macproxy_classic/archive/refs/tags/v$MACPROXY_VER.tar.gz" </dev/null
    tar -xzvf "macproxy_classic-$MACPROXY_VER.tar.gz"

    sudo cp "$MACPROXY_PATH/macproxy.service" "$SYSTEMD_PATH"
    sudo sed -i /^ExecStart=/d "$SYSTEMD_PATH/macproxy.service"
    sudo sed -i "8 i ExecStart=$MACPROXY_PATH/start_macproxy.sh -p=$PORT" "$SYSTEMD_PATH/macproxy.service"
    sudo systemctl daemon-reload
    sudo systemctl enable macproxy
    startMacproxy

    echo -n "Macproxy Classic is now running on IP "
    echo -n `ip -4 addr show scope global | grep -o -m 1 -P '(?<=inet\s)\d+(\.\d+){3}'`
    echo " port $PORT"
    echo "Configure your browser to use the above as http (and https) proxy."
    echo ""
}

# Installs vsftpd (FTP server)
function installFtp() {
    echo
    echo "Installing packages..."
    sudo apt-get install vsftpd --assume-yes --no-install-recommends </dev/null

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
    WEBMIN_VSFTPD_MODULE_VERSION="2024-01-26"

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
    sudo apt-get install webmin --no-install-recommends --assume-yes </dev/null

    rm vsftpd.wbm.gz 2> /dev/null || true
    wget -O vsftpd.wbm.tgz "https://github.com/rdmark/vsftpd-webmin/releases/download/$WEBMIN_VSFTPD_MODULE_VERSION/vsftpd-$WEBMIN_VSFTPD_MODULE_VERSION.wbm.gz" </dev/null
    sudo "$WEBMIN_PATH/install-module.pl" vsftpd.wbm.tgz
    rm vsftpd.wbm.tgz || true

    wget -O netatalk.wbm.tgz "https://github.com/Netatalk/netatalk/releases/download/netatalk-4-2-3/netatalk-4.2.3.wbm.gz" </dev/null
    sudo "$WEBMIN_PATH/install-module.pl" netatalk.wbm.tgz
    rm netatalk.wbm.tgz || true
}

function installScsiexec() {
    sudo apt-get install clang cmake --no-install-recommends --assume-yes </dev/null
    wget -O "$BASE/scsiexec.tar.gz" "https://github.com/BlueSCSI/scsiexec/archive/refs/tags/v0.1.0.tar.gz"
    cd "$BASE" || exit 1
    tar -xzf scsiexec.tar.gz
    rm scsiexec.tar.gz
    cd "$BASE/scsiexec" || exit 1
    cmake -B build
    cmake --build build -j
    sudo cmake --install build --prefix /usr/local
}

# Executes the keyword driven scripts for a particular action in the main menu
function runChoice() {
    case $1 in
          1)
              echo "Installing AFP File Server"
              echo "This script will install and configure Netatalk, including AppleTalk, Bonjour, and its AFP-related systemd services."
              sudoCheck
              installNetatalk
              echo "Installing AFP File Server - Complete!"
          ;;
          2)
              echo "Installing FTP File Server"
              echo "This script will make the following changes to your system:"
              echo " - Install packages with apt-get"
              echo " - Enable the vsftpd systemd service"
              echo "WARNING: The FTP server may transfer unencrypted data over the network."
              echo "Proceed with this installation only if you are on a private, secure network."
              sudoCheck
              installFtp
              echo "Installing FTP File Server - Complete!"
          ;;
          3)
              echo "Installing SMB File Server"
              echo "This script will make the following changes to your system:"
              echo " - Install packages with apt-get"
              echo " - Enable Samba systemd services"
              echo " - Create a directory in the current user's home directory where shared files will be stored"
              echo " - Create a Samba user for the current user"
              sudoCheck
              installSamba
              echo "Installing SMB File Server - Complete!"
          ;;
          4)
              echo "Installing Web Proxy Server"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Add and modify systemd services"
              sudoCheck
              updateAptSources
              stopService "macproxy"
              installMacproxy
              echo "Installing Web Proxy Server - Complete!"
          ;;
          5)
              echo "Install Webmin"
              echo "This script will make the following changes to your system:"
              echo "- Add a 3rd party deb repository"
              echo "- Install and start the Webmin webapp"
              echo "- Install the vsftpd Webmin module"
              installWebmin
              echo "Install Webmin - Complete!"
              echo "The Webmin webapp should now be listening to port 10000 on this system"
          ;;
          6)
              echo "Enabling or disabling PiSCSI back-end authentication"
              echo "This script will make the following changes to your system:"
              echo "- Modify user groups and permissions"
              sudoCheck
              stopService "piscsi"
              configureTokenAuth
              echo "Enabling or disabling PiSCSI back-end authentication - Complete!"
          ;;
          7)
              echo "Installing HFS Mac file system tools and drivers"
              echo "This script will make the following changes to your system:"
              echo "- Install additional packages with apt-get"
              echo "- Compile and install hfsutils and hfdisk from source"
              echo "- Fetch driver files into the PiSCSI data directory"
              sudoCheck
              updateAptSources
              installHfsutils
              installHfdisk
              fetchHardDiskDrivers
              echo "Installing HFS Mac file system tools and drivers - Complete!"
          ;;
          8)
              echo "Installing scsiexec tool"
              echo "This script will make the following changes to your system:"
              echo "- Compile and install scsiexec from source"
              sudoCheck
              updateAptSources
              installScsiexec
              echo "Installing scsiexec tool - Complete!"
          ;;
          *)
              echo "${1} is not a valid option, exiting..."
              exit 1
    esac
}

# Reads and validates the main menu choice
function readChoice() {
    choice=-1

    until [[ $choice -ge 1 && $choice -le 8 ]]; do
        echo -n "Enter your choice (1-8) or CTRL-C to exit: "
        read -r choice
    done

    runChoice "$choice"
}

# Shows the interactive main menu of the script
function showMenu() {
    echo "For command line options, rerun with ./easyinstall.sh --help"
    echo ""
    echo "Choose among the following options:"
    echo "INSTALL/UPDATE PISCSI"
    echo "  Install a binary deb package or use the build system to compile from source."
    echo "  See INSTALL.md for more information."
    echo "INSTALL COMPANION APPS"
    echo "  1) Install AFP File Server (Netatalk)"
    echo "  2) Install FTP File Server (vsftpd)"
    echo "  3) Install SMB File Server (Samba)"
    echo "  4) Install Web Proxy Server (Macproxy Classic)"
    echo "  5) Install Webmin system administration suite"
    echo "ADVANCED OPTIONS"
    echo "  6) Enable or disable PiSCSI token authentication"
    echo "  7) Install HFS Mac file system tools and drivers"
    echo "  8) Install scsiexec tool"
}

# parse arguments passed to the script
while [ "$1" != "" ]; do
    PARAM=$(echo "$1" | awk -F= '{print $1}')
    VALUE=$(echo "$1" | awk -F= '{print $2}')
    case $PARAM in
        -r | --run_choice)
            if ! [[ $VALUE =~ ^[1-8]$ ]]; then
                echo "ERROR: The run choice parameter must have a numeric value between 1 and 8"
                exit 1
            fi
            RUN_CHOICE=$VALUE
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
        -s | --skip_packages)
            SKIP_PACKAGES=1
            ;;
        --help)
            echo "Usage: ./easyinstall.sh [options]"
            echo
            echo "-r=CHOICE, --run_choice=CHOICE        Choose a menu option (1 to 8)"
            echo "-t=TOKEN, --token=TOKEN               Token password for protecting PiSCSI"
            echo "-h, --headless                        Don't ask questions (use with -r=CHOICE)"
            echo "-s, --skip_packages                   Don't install Debian packages"
            exit
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
