#/bin/sh
clear
make all -j16 CXX=clang++
scp ./bin/fullspec/* akuker@10.0.1.98:/home/akuker/
