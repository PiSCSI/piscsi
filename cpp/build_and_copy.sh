#/bin/sh
clear
make scsiloop CROSS_COMPILE=arm-linux-gnueabihf- -j8
scp ./bin/fullspec/* akuker@10.0.1.116:/home/akuker/