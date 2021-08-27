#!/bin/bash

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
  (  ¯¯¯¯¯¯¯  ) RaSCSI Assistant\n
   '~ .~~~. ~'\n
       '~'\n
"""
echo -e $logo
}

VIRTUAL_DRIVER_PATH=/home/pi/images
HFS_FORMAT=/usr/bin/hformat
HFDISK_BIN=/usr/bin/hfdisk
LIDO_DRIVER=~/RASCSI/lido-driver.img
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
GIT_REMOTE=${GIT_REMOTE:-origin}

function initialChecks() {
    currentUser=$(whoami)
    if [ "pi" != "$currentUser" ]; then
        echo "You must use 'pi' user (current: $currentUser)"
        exit 1
    fi

    if [ ! -d ~/RASCSI ]; then
        echo "You must checkout RASCSI repo into /home/pi/RASCSI"
        echo "$ git clone git@github.com:akuker/RASCSI.git"
        exit 2
    fi
}

function installPackages() {
    sudo apt-get update && sudo apt install git libspdlog-dev libpcap-dev genisoimage python3 python3-venv nginx libpcap-dev protobuf-compiler bridge-utils python3-dev libev-dev libevdev2 -y
}

# install all dependency packages for RaSCSI Service
# compile and install RaSCSI Service
function installRaScsi() {
    installPackages

    cd ~/RASCSI/src/raspberrypi
    make all CONNECT_TYPE=FULLSPEC
    sudo make install CONNECT_TYPE=FULLSPEC

    sudoIsReady=$(sudo grep -c "rascsi" /etc/sudoers)

    if [ $sudoIsReady = "0" ]; then
        sudo bash -c 'echo "
# Allow the web server to restart the rascsi service
www-data ALL=NOPASSWD: /bin/systemctl restart rascsi.service
www-data ALL=NOPASSWD: /bin/systemctl stop rascsi.service
# Allow the web server to reboot the raspberry pi
www-data ALL=NOPASSWD: /sbin/shutdown, /sbin/reboot
" >> /etc/sudoers'
    fi

    sudo systemctl restart rsyslog
    sudo systemctl enable rascsi # optional - start rascsi at boot
    sudo systemctl start rascsi
}

function stopOldWebInterface() {
    APACHE_STATUS=$(sudo systemctl status apache2 &> /dev/null; echo $?)
    if [ "$APACHE_STATUS" -eq 0 ] ; then
        echo "Stopping old Apache2 RaSCSI Web..."
        sudo systemctl disable apache2
        sudo systemctl stop apache2
    fi
}

# install everything required to run an HTTP server (Nginx + Python Flask App)
function installRaScsiWebInterface() {
    stopOldWebInterface
    installPackages

    sudo cp -f ~/RASCSI/src/web/service-infra/nginx-default.conf /etc/nginx/sites-available/default
    sudo cp -f ~/RASCSI/src/web/service-infra/502.html /var/www/html/502.html

    mkdir -p $VIRTUAL_DRIVER_PATH
    chmod -R 775 $VIRTUAL_DRIVER_PATH
    groups www-data
    sudo usermod -a -G pi www-data
    groups www-data

    sudo systemctl reload nginx

    sudo cp ~/RASCSI/src/web/service-infra/rascsi-web.service /etc/systemd/system/rascsi-web.service
    sudo systemctl daemon-reload
    sudo systemctl enable rascsi-web
    sudo systemctl start rascsi-web
}

function updateRaScsiGit() {
    echo "Updating checked out branch $GIT_REMOTE/$GIT_BRANCH"
    cd ~/RASCSI
    stashed=0
    if [[ $(git diff --stat) != '' ]]; then
        echo 'There are local changes, we will stash and reapply them.'
        git stash
        stashed=1
    fi

    git fetch $GIT_REMOTE
    git rebase $GIT_REMOTE/$GIT_BRANCH

    if [ $stashed -eq 1 ]; then
        echo "Reapplying local changes..."
        git stash apply
    fi
}

function updateRaScsi() {
    updateRaScsiGit
    installPackages
    sudo systemctl stop rascsi

    cd ~/RASCSI/src/raspberrypi

    make clean
    make all CONNECT_TYPE=FULLSPEC
    sudo make install CONNECT_TYPE=FULLSPEC
    sudo systemctl start rascsi
}

function updateRaScsiWebInterface() {
    stopOldWebInterface
    updateRaScsiGit
    sudo cp -f ~/RASCSI/src/web/service-infra/nginx-default.conf /etc/nginx/sites-available/default
    sudo cp -f ~/RASCSI/src/web/service-infra/502.html /var/www/html/502.html
    echo "Restarting rascsi-web services..."
    sudo systemctl restart rascsi-web
    sudo systemctl restart nginx
}

function detectRaScsiInstall() {
    if [ -x /usr/local/bin/rascsi ]; then
        echo "RaSCSI is already installed"
        return 0
    else
        echo "RaSCSI is not installed"
        return 1
    fi
}

function showRaScsiStatus() {
    sudo systemctl status rascsi | tee
}

function createDrive600MB() {
    createDrive 600 "HD600"
}

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

function formatDrive() {
    diskPath="$1"
    volumeName="$2"

    if [ ! -x $HFS_FORMAT ]; then
        # Install hfsutils to have hformat to format HFS
        sudo apt-get install hfsutils
    fi

    if [ ! -x $HFDISK_BIN ]; then
        # Clone, compile and install 'hfdisk', partition tool
        git clone git://www.codesrc.com/git/hfdisk.git
        cd hfdisk
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
        if [ ! -f $LIDO_DRIVER ];then
            echo "Lido driver couldn't be found. Make sure RaSCSI is up-to-date with git pull"
            return 1
        fi

        # Burn Lido driver to the disk
        dd if=$LIDO_DRIVER of="$diskPath" seek=64 count=32 bs=512 conv=notrunc

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

function createDrive() {
    if [ $# -ne 2 ]; then
        echo "To create a Drive, volume size and volume name must be provided"
        echo "$ createDrive 600 \"RaSCSI Drive\""
        echo "Drive wasn't created."
        return
    fi

    driveSize=$1
    driveName=$2
    mkdir -p $VIRTUAL_DRIVER_PATH
    drivePath="${VIRTUAL_DRIVER_PATH}/${driveSize}MB.hda"

    if [ ! -f $drivePath ]; then
        echo "Creating a ${driveSize}MB Drive"
        truncate --size ${driveSize}m $drivePath

        echo "Formatting drive with HFS"
        formatDrive "$drivePath" "$driveName"

    else
        echo "Error: drive already exists"
    fi
}

function runChoice() {
  case $1 in
          0)
              echo "Installing RaSCSI Service + Web interface"
              installRaScsi
              installRaScsiWebInterface
              createDrive600MB
              showRaScsiStatus
              echo "Installing RaSCSI Service + Web interface - Complete!"
          ;;
          1)
              echo "Installing RaSCSI Service"
              installRaScsi
              showRaScsiStatus
              echo "Installing RaSCSI Service - Complete!"
          ;;
          2)
              echo "Installing RaSCSI Web interface"
              installRaScsiWebInterface
              echo "Installing RaSCSI Web interface - Complete!"
          ;;
          3)
              echo "Updating RaSCSI Service + Web interface"
              updateRaScsi
              updateRaScsiWebInterface
              showRaScsiStatus
              echo "Updating RaSCSI Service + Web interface - Complete!"
          ;;
          4)
              echo "Updating RaSCSI Service"
              updateRaScsi
              showRaScsiStatus
              echo "Updating RaSCSI Service - Complete!"
          ;;
          5)
              echo "Updating RaSCSI Web interface"
              updateRaScsiWebInterface
              echo "Updating RaSCSI Web interface - Complete!"
          ;;
          6)
              echo "Creating a 600MB drive"
              createDrive600MB
              echo "Creating a 600MB drive - Complete!"
          ;;
          7)
              echo "Creating a custom drive"
              createDriveCustom
              echo "Creating a custom drive - Complete!"
          ;;
          8)
              checkIfNetworkIsConfigured
          ;;
          -h|--help|h|help)
              showMenu
          ;;
          *)
              echo "${1} is not a valid option, exiting..."
              exit 1
          ;;
      esac
}

function readChoice() {
   choice=-1

   until [ $choice -ge "0" ] && [ $choice -le "8" ]; do
       echo -n "Enter your choice (0-8) or CTRL-C to exit: "
       read -r choice
   done

   runChoice "$choice"
}


function setupWiredNetworking() {
    echo "Setting up wired network..."
    # Setup bridge
    cp ~/RASCSI/src/raspberrypi/os_integration/rascsi_bridge /etc/network/interfaces.d/rascsi_bridge

    # Setup routing
    grep "^denyinterfaces eth0" || echo "denyinterfaces eth0" >> /etc/dhcpcd.conf

    # Setup daynaPORT at start up
    sudo sed s@"^ExecStart=/usr/local/bin/rascsi"@"ExecStart=/usr/local/bin/rascsi -ID2 daynaport"@ /etc/systemd/system/rascsi.service > /etc/systemd/system/rascsi.service

    echo "We need to reboot your Pi"
    echo "Press Enter to reboot or CTRL-C to exit"
    read

    echo "Rebooting..."
    sleep 3
    sudo reboot
}

function setupWirelessNetworking() {
    # IP=`ip route get 1.2.3.4 | awk '{print $7}' | head -n 1`

    IP="10.10.20.2" # Macintosh or Device IP
    NETWORK="`echo $IP | cut -d"." -f1`.`echo $IP | cut -d"." -f2`.`echo $IP | cut -d"." -f3`"
    NETWORK_MASK=$NETWORK.0/24
    ROUTER_IP=$NETWORK.1
    # IP Address
    echo "Bridge network settings, please verify:"
    echo "Macintosh or Device IP: $IP"
    echo "ROUTER_IP: $ROUTER_IP"
    echo "NETWORK_MASK: $NETWORK_MASK"

    echo "Press enter to continue or CTRL-C to exit"
    read REPLY

    # Installing required packages
    echo "Installing required packages (pytun)"
    sudo apt-get install -y python3-pip 
    sudo pip3 install python-pytun

    # Configuring bridge
    grep "rascsi_bridge" /etc/rc.local || echo "
brctl addbr rascsi_bridge
ifconfig rascsi_bridge $ROUTER_IP/24 up
" >> /etc/rc.local

    grep "^net.ipv4.ip_forward=1" /etc/sysctl.conf || sudo echo "net.ipv4.ip_forward=1" >> /etc/sysctl.conf
    
    sudo iptables --flush
    sudo iptables -t nat -F
    sudo iptables -X
    sudo iptables -Z
    sudo iptables -P INPUT ACCEPT
    sudo iptables -P OUTPUT ACCEPT
    sudo iptables -P FORWARD ACCEPT
    sudo iptables -t nat -A POSTROUTING -o wlan0 -s $NETWORK_MASK -j MASQUERADE


    sudo apt-get install iptables-persistent --force-yes
    # #### Below are the rules you should have in rules.v4 ####
    # Generated by xtables-save v1.8.2 on Thu Aug 26 10:41:38 2021
    # *nat
    # :PREROUTING ACCEPT [0:0]
    # :INPUT ACCEPT [0:0]
    # :POSTROUTING ACCEPT [0:0]
    # :OUTPUT ACCEPT [0:0]
    # -A POSTROUTING -s 10.10.20.0/24 -o wlan0 -j MASQUERADE
    # COMMIT
    # ##############################################
    # it is possible to re-save the rules with:
    # iptables-save > /etc/iptables/rules.v4

    echo "We need to reboot your Pi"
    echo "Press Enter to reboot or CTRL-C to exit"
    read REPLY

    echo "Rebooting..."
    sleep 3
    sudo reboot
}


function checkNetworkType() {
    HAS_WIFI=$(ifconfig | grep -c wlan0)
    HAS_WIRE=$(ifconfig | grep -c eth)

    if [ $HAS_WIFI -eq 0 ] && [ $HAS_WIRE -eq 0 ]; then
        echo "No network interface found. Please configure your network interface first."
    elif [ $HAS_WIFI -eq 1 ]; then
        echo "Wifi detected (Wireless)"
        setupWirelessNetworking
    elif [ $HAS_WIRE -eq 1 ]; then
        echo "Ethernet detected (Wired)"
        setupWiredNetworking
    fi
}


function checkIfNetworkIsConfigured() {
    echo "Checking current Network status"
    ROUTING=$(grep -c "10.10.20.0/24 -o wlan0" /etc/iptables/rules.v4)
    BRIDGE=$(brctl show | grep -c "rascsi_bridge")
    FORWARD=$(grep -c "^net.ipv4.ip_forward=1" /etc/sysctl.conf)

    echo "Routing: $ROUTING / Bridge: $BRIDGE / Forward: $FORWARD"

    if [ $ROUTING -ge 1 ]; then
        echo "Network routing seems to be configured."
    fi
    if [ $BRIDGE -ge 1 ]; then
        echo "Network bridgeseems to be configured."
    fi
    if [ $FORWARD -ge 1 ]; then
        echo "IP forwardingseems to be configured."
    fi

    if [ $ROUTING -ge 1 ] || [ $BRIDGE -ge 1 ] || [ $FORWARD -ge 1 ]; then
        echo "Are you sure you want to proceed with network configuration? Y/n"
        read REPLY

        if [ "$REPLY" == "Y" ] || [ "$REPLY" == "y" ]; then
            echo "Proceeding with network configuration..."
            checkNetworkType
        fi
    fi
}

function showMenu() {
    echo ""
    echo "Choose among the following options:"
    echo "INSTALL"
    echo "  0) install RaSCSI Service + web interface + 600MB Drive (recommended)"
    echo "  1) install RaSCSI Service (initial)"
    echo "  2) install RaSCSI Web interface"
    echo "UPDATE"
    echo "  3) update RaSCSI Service + web interface (recommended)"
    echo "  4) update RaSCSI Service"
    echo "  5) update RaSCSI Web interface"
    echo "CREATE EMPTY DRIVE"
    echo "  6) 600MB drive (recommended)"
    echo "  7) custom drive size (up to 4000MB)"
    echo "NETWORK"
    echo "  8) Network assistant (wire and wireless)"
}


showRaSCSILogo
initialChecks
if [ -z "${1}" ]; then # $1 is unset, show menu
    showMenu
    readChoice
else
    runChoice "$1"
fi

