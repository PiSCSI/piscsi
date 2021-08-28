#!/bin/bash

# BSD 3-Clause License
# Author @sonique6784
# Copyright (c) 2020, sonique6784


VIRTUAL_DRIVER_PATH=/home/pi/images

RASCSI_PATH=~/RASCSI
RASCSI_PATH_SRC=$RASCSI_PATH/src

function initialChecks() {
    currentUser=$(whoami)
    if [ "pi" != $currentUser ]; then
        echo "You must use 'pi' user (current: $currentUser)"
        exit 1
    fi

    if [ ! -d ~/RASCSI ]; then
        echo "You must checkout RASCSI repo into /user/pi/RASCSI"
        exit 2
    fi
}


# install all dependency packages for RaSCSI Service
# compile and install RaSCSI Service
function installRaScsi() {
    sudo apt-get update && sudo apt-get install --yes git libspdlog-dev

	cd $RASCSI_PATH_SRC/raspberrypi
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

# install everything required to run an HTTP server (Apache+PHP)
# configure PHP
# install 
function installRaScsiWebInterface() {
	
	sudo apt install apache2 php libapache2-mod-php -y
	
    sudo cp $RASCSI_PATH_SRC/php/* /var/www/html


    PHP_CONFIG_FILE=/etc/php/7.3/apache2/php.ini

    #Comment out any current configuration
    sudo sed -i.bak 's/^post_max_size/#post_max_size/g' $PHP_CONFIG_FILE
    sudo sed -i.bak 's/^upload_max_filesize/#upload_max_filesize/g' $PHP_CONFIG_FILE

    sudo bash -c 'PHP_CONFIG_FILE=/etc/php/7.3/apache2/php.ini && echo "
# RaSCSI high upload limits
upload_max_filesize = 1200M
post_max_size = 1200M

" >> $PHP_CONFIG_FILE'

    mkdir -p $VIRTUAL_DRIVER_PATH
    chmod -R 775 $VIRTUAL_DRIVER_PATH
    groups www-data
    sudo usermod -a -G pi www-data
    groups www-data

    sudo /etc/init.d/apache2 restart
}



function updateRaScsi() {
    sudo systemctl stop rascsi

	cd $RASCSI_PATH_SRC/raspberrypi
	
	make clean
	make all CONNECT_TYPE=FULLSPEC
	sudo make install CONNECT_TYPE=FULLSPEC
	sudo systemctl start rascsi
}

function updateRaScsiWebInterface() {
    sudo /etc/init.d/apache2 stop
    cd $RASCSI_PATH
    git fetch --all
	cd $RASCSI_PATH_SRC/raspberrypi 
    sudo cp $RASCSI_PATH_SRC/php/* /var/www/html

    sudo /etc/init.d/apache2 start
}

function showRaScsiStatus() {
    sudo systemctl status rascsi
}

function createDrive600MB() {
    createDrive 600
}

function createDriveCustom() {
    driveSize=-1
    until [ $driveSize -ge "10" ] && [ $driveSize -le "4000" ]; do
        echo "What drive size would you like (in MB) (10-4000)"
        read driveSize
    done

    createDrive $driveSize
}

function createDrive() {
    driveSize=$1
    mkdir -p $VIRTUAL_DRIVER_PATH
    drivePath="${VIRTUAL_DRIVER_PATH}/${driveSize}MB.hda"
    echo $drivePath
    if [ ! -f $drivePath ]; then
        echo "Creating a ${driveSize}MB Drive"
        dd if=/dev/zero of=$drivePath bs=1M count=$driveSize
    else
        echo "Error: drive already exists"
    fi
}


initialChecks


echo "Welcome to Easy Install for RaSCSI"
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


choice=-1

until [ $choice -ge "0" ] && [ $choice -le "7" ]; do
    echo "Enter your choice (0-7) or CTRL-C to exit"
    read choice
done


case $choice in
    0)
        echo "Installing RaSCSI Service + Web interface"
        installRaScsi
        installRaScsiWebInterface
        createDrive600MB
        showRaScsiStatus
    ;;
    1)
        echo "Installing RaSCSI Service"
        installRaScsi
        showRaScsiStatus
    ;;
    2)
        echo "Installing RaSCSI Web interface"
        installRaScsiWebInterface
    ;;
    3)
        echo "Updating RaSCSI Service + Web interface"
        updateRaScsi
        updateRaScsiWebInterface
        showRaScsiStatus
    ;;
    4)
        echo "Updating RaSCSI Service"
        updateRaScsi
        showRaScsiStatus
    ;;
    5)
        echo "Updating RaSCSI Web interface"
        updateRaScsiWebInterface
    ;;
    6)
        echo "Creating a 600MB drive"
        createDrive600MB
    ;;
    7)
        echo "Creating a custom drive"
        createDriveCustom
    ;;
esac


