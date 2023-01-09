#
# Makefile for RASCTL.X
#

CC = gcc -O2 -Wall
LK = hlk  -l -x

RASCTL.X	:	RASCTL.o
	$(LK) -o $@ $^ libc.a libiocs.a libscsi.a libgnu.a

RASCTL.o		:	RASCTL.C
	$(CC) -c -o $@ $^
