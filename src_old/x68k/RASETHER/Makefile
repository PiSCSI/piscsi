#
# Makefile for RASETHER.SYS
#

TARGET = RASETHER.SYS

OBJS	 = re.o asmsub.o main.o scsictl.o
LIBS	 = libc.a libgnu.a libscsi.a

CC = gcc -O2 -Wall -fomit-frame-pointer
CFLAGS = -D__DOS_INLINE__ -D__IOCS_INLINE__
#CFLAGS = -D__DOS_INLINE__ -D__IOCS_INLINE__ -DUSE_DMA

AS = has
ASFLAGS =
LD = hlk
LDFLAGS = -l -x

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

scsictl.o: scsictl.c scsictl.h
	$(COMPILE.c) $<

main.o: main.c main.h scsictl.h
	$(COMPILE.c) $<

asmsub.o: asmsub.s

# EOF
