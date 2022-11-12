#/bin/sh
clear
make all CROSS_COMPILE=arm-linux-gnueabihf- -j8
scp ./bin/fullspec/* akuker@10.0.1.98:/home/akuker/
