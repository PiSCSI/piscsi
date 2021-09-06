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

# parse arguments
while [ "$1" != "" ]; do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -c | --connect_type)
            CONNECT_TYPE=$VALUE
            ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            exit 1
            ;;
    esac
    shift
done

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

# install all dependency packages for RaSCSI Service
function installPackages() {
    sudo apt-get update && sudo apt install git libspdlog-dev libpcap-dev genisoimage python3 python3-venv nginx libpcap-dev protobuf-compiler bridge-utils python3-dev libev-dev libevdev2 -y
}

# compile and install RaSCSI Service
function installRaScsi() {
    sudo systemctl stop rascsi

    cd ~/RASCSI/src/raspberrypi
    make clean
    make all CONNECT_TYPE=${CONNECT_TYPE-FULLSPEC}
    sudo make install CONNECT_TYPE=${CONNECT_TYPE-FULLSPEC}

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

# install everything required to run an HTTP server (Nginx + Python Flask App)
function installRaScsiWebInterface() {
    echo "Compiling the Python protobuf library..."
    [ -f ~/RASCSI/src/web/rascsi_interface.proto ] && rm ~/RASCSI/src/web/rascsi_interface.proto
    protoc -I=/home/pi/RASCSI/src/raspberrypi/ --python_out=/home/pi/RASCSI/src/web/ rascsi_interface.proto

    sudo cp -f ~/RASCSI/src/web/service-infra/nginx-default.conf /etc/nginx/sites-available/default
    sudo cp -f ~/RASCSI/src/web/service-infra/502.html /var/www/html/502.html

    sudo usermod -a -G pi www-data

    sudo systemctl reload nginx

    if [ -f /etc/systemd/system/rascsi-web.service ]; then
	echo "Found an existing rascsi-web configuration; backing up to rascsi-web.service.old..."
	sudo cp /etc/systemd/system/rascsi-web.service /etc/systemd/system/rascsi-web.service.old
    else
	echo "Installing the rascsi-web.service configuration..."
	sudo cp ~/RASCSI/src/web/service-infra/rascsi-web.service /etc/systemd/system/rascsi-web.service
    fi

    sudo systemctl daemon-reload
    sudo systemctl enable rascsi-web
    sudo systemctl start rascsi-web
}

function createImagesDir() {
    if [ -d $VIRTUAL_DRIVER_PATH ]; then
        echo "The $VIRTUAL_DRIVER_PATH directory already exists."
    else
	echo "The $VIRTUAL_DRIVER_PATH directory does not exist; creating..."
	mkdir -p $VIRTUAL_DRIVER_PATH
        chmod -R 775 $VIRTUAL_DRIVER_PATH
    fi
}

function stopOldWebInterface() {
    sudo systemctl stop rascsi-web
    APACHE_STATUS=$(sudo systemctl status apache2 &> /dev/null; echo $?)
    if [ "$APACHE_STATUS" -eq 0 ] ; then
        echo "Stopping old Apache2 RaSCSI Web..."
        sudo systemctl disable apache2
        sudo systemctl stop apache2
    fi
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
              echo "Installing RaSCSI Service + Web interface + 600MB Drive"
              stopOldWebInterface
              updateRaScsiGit
              createImagesDir
              installPackages
              installRaScsi
              installRaScsiWebInterface
              createDrive600MB
              showRaScsiStatus
              echo "Installing RaSCSI Service + Web interface + 600MB Drive - Complete!"
          ;;
          1)
              echo "Installing RaSCSI Service + Web interface"
              stopOldWebInterface
              updateRaScsiGit
              createImagesDir
              installPackages
              installRaScsi
              installRaScsiWebInterface
              showRaScsiStatus
              echo "Installing RaSCSI Service + Web interface - Complete!"
          ;;
          2)
              echo "Installing RaSCSI Service"
              updateRaScsiGit
              createImagesDir
              installPackages
              installRaScsi
              showRaScsiStatus
              echo "Installing RaSCSI Service - Complete!"
          ;;
          3)
              echo "Creating a 600MB drive"
              createDrive600MB
              echo "Creating a 600MB drive - Complete!"
          ;;
          4)
              echo "Creating a custom drive"
              createDriveCustom
              echo "Creating a custom drive - Complete!"
          ;;
          -h|--help|h|help)
              showMenu
          ;;
          *)
              echo "${1} is not a valid option, exiting..."
              exit 1
      esac
}

function readChoice() {
   choice=-1

   until [ $choice -ge "0" ] && [ $choice -le "7" ]; do
       echo -n "Enter your choice (0-7) or CTRL-C to exit: "
       read -r choice
   done

   runChoice "$choice"
}

function showMenu() {
    echo ""
    echo "Choose among the following options:"
    echo "INSTALL/UPDATE RASCSI"
    echo "  0) install or update RaSCSI Service + web interface + 600MB Drive (recommended)"
    echo "  1) install or update RaSCSI Service + web interface"
    echo "  2) install or update RaSCSI Service"
    echo "CREATE EMPTY DRIVE IMAGE"
    echo "  3) 600MB drive (recommended)"
    echo "  4) custom drive size (up to 4000MB)"
}


showRaSCSILogo
initialChecks
if [ -z "${1}" ]; then # $1 is unset, show menu
    showMenu
    readChoice
else
    runChoice "$1"
fi
