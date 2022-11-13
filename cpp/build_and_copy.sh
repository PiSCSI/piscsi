#/bin/sh
clear
make all -j16 CXX=clang++ EXTRA_FLAGS=--target=arm-linux-gnueabihf
#scp ./bin/fullspec/* akuker@10.0.1.98:/home/akuker/
scp ./bin/fullspec/* akuker@10.0.1.13:/home/akuker/
scp ./bin/fullspec/* akuker@10.0.1.9:/home/akuker/
