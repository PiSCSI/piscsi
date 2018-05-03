#
# Makefile for RASDRV.SYS
#

CC = gcc -O2 -Wall -fomit-frame-pointer
AS = has
LK = hlk  -l -x

RASDRV.SYS	:	RASDRV.o BRIDGE.o
	$(LK) -o $@ $^ libc.a libiocs.a libscsi.a

RASDRV.o		:	RASDRV.S
	$(AS) $^

BRIDGE.o		:	BRIDGE.C
	$(CC) -c -o $@ $^
