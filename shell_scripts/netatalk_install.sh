#!/usr/bin/env bash

# Netatalk install script for Debian GNU/Linux.
#
# By Daniel Markstedt
# Based on RsSCSI easyinstall.sh by sonique6784
# BSD 3-Clause License
# Copyright (c) 2022, 2023, 2025, Daniel Markstedt
# Copyright (c) 2020, sonique6784

set -e

USER=$(whoami)
BASE=$(dirname "$(readlink -f "${0}")")
CORES=$(nproc)
SYSCONFDIR="/etc"
NETATALK_CONFDIR="$SYSCONFDIR/netatalk"
INSTALL_PACKAGES=1
MAKE_CLEAN=1
START_SERVICES=1
INTERACTIVE=1

# checks to run before installation
function initialChecks() {
    if [ "root" == "$USER" ]; then
        echo "Do not run this script as $USER or with 'sudo'."
        exit 1
    fi

    echo "Netatalk install script for Debian Linux."
    echo "It attempts to set up a universally compatible AFP server:"

    if [[ "$AFP_SHARE_PATH" ]]; then
        echo " - One shared volume named '$AFP_SHARE_NAME' ($AFP_SHARE_PATH)"
    else
        echo " - Shared volume is the home directory of the authenticated user"
    fi

    echo " - Classic AppleTalk (DDP) support enabled"
    echo " - TCP/IP (DSI) support and service discovery with Zeroconf / Bonjour enabled"
    echo " - DHX2, DHX, ClearTxt, and Guest UAMs to allow any AFP client to authenticate"
    echo " - Additional AppleTalk daemons papd (printer server), timelord (time server), and a2boot (Apple II netboot server)"
    echo
    echo "The following changes will be made to your system:"
    echo " - Modify user groups and permissions"
    echo " - Install packages with apt-get"
    echo " - Install Netatalk systemd services: netatalk, atalkd, cnid, papd, timelord, a2boot"
    echo " - Create a directory in the current user's home directory where shared files will be stored"
    echo " - Install binaries to /usr/local/sbin"
    echo " - Install manpages to /usr/local/share/man"
    echo " - Install configuration files to $SYSCONFDIR"
    echo " - Install the CUPS printing system and modify its configuration"
    echo
    echo "Input your password to allow this script to make the above changes."
    sudo -v

    echo
    echo "IMPORTANT: "$USER" needs to have a password of 8 chars or less due to Classic Mac OS limitations."
    echo "Do you want to change your password now? [y/N]"
    read -r REPLY
    if [ "$REPLY" == "y" ] || [ "$REPLY" == "Y" ]; then
        passwd
    fi
}

function installNetatalk() {
    echo "Checking for previous versions of Netatalk..."
    sudo systemctl stop atalkd afpd netatalk || true

    if [ -f /etc/init.d/netatalk ]; then
        echo
        echo "WARNING: Legacy init scripts for a previous version of Netatalk was detected on your system. It is recommended to back up you configuration files and shared files before proceeding. Press CTRL-C to exit, or any other key to proceed."
        read
        sudo /etc/init.d/netatalk stop || true
    fi

    if [ -f /var/log/afpd.log ]; then
        echo "Cleaning up /var/log/afpd.log..."
        sudo rm /var/log/afpd.log
    fi

    if [[ `grep -c netatalk /etc/rc.local` -eq 1 ]]; then
        sudo sed -i "/netatalk/d" /etc/rc.local
        echo "Removed Netatalk from /etc/rc.local -- use systemctl to control Netatalk from now on."
    fi

    if [[ "$AFP_SHARE_PATH" ]]; then
        if [ -d "$AFP_SHARE_PATH" ]; then
            echo "Found a $AFP_SHARE_PATH directory; will use it for file sharing."
        else
            echo "Creating the $AFP_SHARE_PATH directory and granting read/write permissions to all users..."
            sudo mkdir -p "$AFP_SHARE_PATH"
            sudo chown -R "$USER:$USER" "$AFP_SHARE_PATH"
            chmod -R 2775 "$AFP_SHARE_PATH"
        fi
    fi

    if [ $INSTALL_PACKAGES ]; then
        echo
        echo "Installing dependencies..."
        sudo apt-get update || true
        sudo apt-get install cmark-gfm gcc libdb-dev libcups2-dev cups libavahi-client-dev libevent-dev libgcrypt20-dev libiniparser-dev libpam0g-dev libsqlite3-dev meson ninja-build pkg-config --assume-yes --no-install-recommends </dev/null
    fi

    cd "$BASE" || exit 1

    echo
    echo "Configuring Netatalk..."
    meson setup build -Dbuildtype=release -Dwith-appletalk=true -Dwith-cups-pap-backend=true -Dwith-pkgconfdir-path="$SYSCONFDIR" -Dwith-webmin=true

    echo
    echo "Compiling Netatalk..."
    meson compile -C build

    echo
    echo "Installing Netatalk..."
    sudo meson install -C build

    if [[ `lsmod | grep -c appletalk` -eq 0 ]]; then
        echo
        echo "Your system may not have support for AppleTalk networking."
        echo "You can still use Netatalk with Macs that support AppleTalk over TCP/IP (DSI)."
        echo "In the Chooser, input the IP address of the network interface that is connected to the rest of your network:"
        echo `ip -4 addr show scope global | grep -oP '(?<=inet\s)\d+(\.\d+){3}'`
    fi

    echo
    echo "Modifying service configurations..."

    AFPCONF="$NETATALK_CONFDIR/afp.conf"

    if ! grep -q "^uam list" "$AFPCONF"; then
        sudo sed -i "/^\[Global\]/a uam list = uams_clrtxt.so uams_guest.so uams_dhx.so uams_dhx2.so" "$AFPCONF"
        echo "Added 'uam list' to [Global] in afp.conf"
    else
        echo "'uam list' already exists; not updating afp.conf"
    fi

    if [[ "$AFP_SHARE_NAME" && "$AFP_SHARE_PATH" ]]; then
        if ! grep -q "^\[$AFP_SHARE_NAME\]" "$AFPCONF"; then
            {
                echo
                echo "[$AFP_SHARE_NAME]"
                echo "path = $AFP_SHARE_PATH"
                echo "volume name = $AFP_SHARE_NAME"
            } | sudo tee -a "$AFPCONF" > /dev/null
            echo "Added share section for $AFP_SHARE_NAME to afp.conf"
        else
            echo "Share section [$AFP_SHARE_NAME] already exists; not updating afp.conf"
        fi
    fi

    if [[ "$ADDITIONAL_SHARE_NAME" && "$ADDITIONAL_SHARE_PATH" ]]; then
        if ! grep -q "^\[$ADDITIONAL_SHARE_NAME\]" "$AFPCONF"; then
            {
                echo
                echo "[$ADDITIONAL_SHARE_NAME]"
                echo "path = $ADDITIONAL_SHARE_PATH"
                echo "volume name = $ADDITIONAL_SHARE_NAME"
            } | sudo tee -a "$AFPCONF" > /dev/null
            echo "Added share section for $ADDITIONAL_SHARE_NAME to afp.conf"
        else
            echo "Share section [$ADDITIONAL_SHARE_NAME] already exists; not updating afp.conf"
        fi
    fi

    if [[ "$APPLETALK_INTERFACE" ]]; then
        echo "$NETATALK_CONFDIR/atalkd.conf:"
        echo "$APPLETALK_INTERFACE" | sudo tee -a "$NETATALK_CONFDIR/atalkd.conf"
    fi

    echo "$NETATALK_CONFDIR/papd.conf:"
    echo "cupsautoadd:op=root:" | sudo tee -a "$NETATALK_CONFDIR/papd.conf"
    sudo usermod -a -G lpadmin $USER
    sudo cupsctl --remote-admin WebInterface=yes
    if [[ `sudo grep -c "PreserveJobHistory" /etc/cups/cupsd.conf` -eq 0 ]]; then
        echo "/etc/cups/cupsd.conf:"
        sudo sed -i "/MaxLogSize/a PreserveJobHistory\ No" /etc/cups/cupsd.conf
    fi

    if [ $START_SERVICES ]; then
        echo
        echo "Starting systemd services... (this may take a while)"
        sudo systemctl start netatalk atalkd papd timelord a2boot cups

        echo
        echo "Netatalk daemons are now installed and running, and should be discoverable by your Macs."
        echo "To authenticate with the file server, use the current username ("$USER") and password."
        echo
        echo "To learn more about Netatalk and its capabilities, visit https://netatalk.io"
        echo "Enjoy AFP file sharing!"
        echo
    fi

    if [ -f $NETATALK_CONFDIR/afpd.conf ]; then
        echo
        echo "NOTE: An older Netatalk 2.x installation was detected."
        echo "Please review and merge any custom settings from $NETATALK_CONFDIR/afpd.conf and $NETATALK_CONFDIR/AppleVolumes.default into $NETATALK_CONFDIR/afp.conf as needed."
    fi
}

function showLogo() {
    echo
    echo "        #             #"
    echo "       ##             ##"
    echo "      # #             # #"
    echo "      # #             # #"
    echo "     #  #             #  #"
    echo "     #  #    #####    #  #"
    echo "     #   # ##     ## #   #"
    echo "     #    #         #    #"
    echo "      #    # ## ## #    #"
    echo "       #       #       #"
    echo "      # #             # #"
    echo "      # #  ###   ###  # #"
    echo "     #    #   # #   #    #"
    echo "     #   #     #     #   #"
    echo "     #   #     #     #   #"
    echo "     #   #     #     #   #"
    echo "     #   #  #  #  #  #   #"
    echo "    ###  # # # # # # #  ###"
    echo "   #     # ### # ### #     #"
    echo "   #      ##### #####      #"
    echo " #############   ##############"
    echo "  #          #   #           #"
    echo "   ###########   ############"
    echo "             #####"
    echo "             #   #"
    echo "              ###"
    echo "             #   #"
    echo "# # #########  #  ########## # #"
    echo "              # #"
    echo "# # ##########   ########### # #"
    echo
}

# parse arguments passed to the script
while [ "$1" != "" ]; do
    PARAM=$(echo "$1" | awk -F= '{print $1}')
    VALUE=$(echo "$1" | awk -F= '{print $2}')
    case $PARAM in
        -b | --base-dir)
            BASE=$VALUE
            ;;
        -d | --sysconf-dir)
            SYSCONFDIR=$VALUE
            ;;
        -n | --share-name)
            AFP_SHARE_NAME=$VALUE
            ;;
        -p | --share-path)
            AFP_SHARE_PATH=$VALUE
            ;;
        -t | --appletalk-interface)
            APPLETALK_INTERFACE=$VALUE
            ;;
        -a | --no-packages)
            INSTALL_PACKAGES=
            ;;
        -c | --no-make-clean)
            MAKE_CLEAN=
            ;;
        -s | --no-start)
            START_SERVICES=
            ;;
        -h | --headless)
            INTERACTIVE=
            ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            exit 1
            ;;
    esac
    shift
done

showLogo
if [ $INTERACTIVE ]; then
    initialChecks
fi
installNetatalk
