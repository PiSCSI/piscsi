.DEFAULT_GOAL: all

CC       = gcc
CXX      = g++

CFLAGS   = -DDISK_LOG -O0 -g -Wall
CXXFLAGS = -DDISK_LOG -O0 -g -Wall



# If its not specified, build for STANDARD configuration
CONNECT_TYPE ?= STANDARD

ifdef CONNECT_TYPE
CFLAGS   += -DCONNECT_TYPE_$(CONNECT_TYPE)
CXXFLAGS += -DCONNECT_TYPE_$(CONNECT_TYPE)
endif

RASCSI = rascsi
RASCTL = rasctl
RASDUMP = rasdump
SASIDUMP = sasidump
SCSIMON = scsimon

#BIN_ALL = $(RASCSI) $(RASCTL) $(RASDUMP) $(SASIDUMP) $(SCSIMON)
# Temporarily remove the RASDUMP and RASDUMP tools, since they're not needed
# for my specific use case. If you need them - add them back in!
BIN_ALL = $(RASCSI) $(RASCTL) $(SCSIMON)


SRC_RASCSI = \
	rascsi.cpp \
	scsi.cpp \
	disk.cpp \
	gpiobus.cpp \
	ctapdriver.cpp \
	cfilesystem.cpp \
	filepath.cpp \
	fileio.cpp

SRC_RASCTL = \
	rasctl.cpp

SRC_RASDUMP = \
	rasdump.cpp \
	scsi.cpp \
	gpiobus.cpp \
	filepath.cpp \
	fileio.cpp

SRC_SASIDUMP = \
	sasidump.cpp \
	scsi.cpp \
	gpiobus.cpp \
	filepath.cpp \
	fileio.cpp

SRC_SCSIMON = \
	scsimon.cpp \
	scsi.cpp \
	disk.cpp \
	gpiobus.cpp \
	ctapdriver.cpp \
	cfilesystem.cpp \
	filepath.cpp \
	fileio.cpp \
	scsimondev.cpp

OBJ_RASCSI := $(SRC_RASCSI:%.cpp=%.o)
OBJ_RASCTL := $(SRC_RASCTL:%.cpp=%.o)
OBJ_RASDUMP := $(SRC_RASDUMP:%.cpp=%.o)
OBJ_SASIDUMP := $(SRC_SASIDUMP:%.cpp=%.o)
OBJ_SCSIMON  := $(SRC_SCSIMON:%.cpp=%.o)
OBJ_ALL := $(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_RASDUMP) $(OBJ_SASIDUMP) $(OBJ_SCSIMON)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

ALL: $(BIN_ALL)
all: $(BIN_ALL)

$(RASCSI): $(OBJ_RASCSI)
	$(CXX) -o $@ $(OBJ_RASCSI) -lpthread

$(RASCTL): $(OBJ_RASCTL)
	$(CXX) -o $@ $(OBJ_RASCTL)

$(RASDUMP): $(OBJ_RASDUMP)
	$(CXX) -o $@ $(OBJ_RASDUMP)

$(SASIDUMP): $(OBJ_SASIDUMP)
	$(CXX) -o $@ $(OBJ_SASIDUMP)

$(SCSIMON): $(OBJ_SCSIMON)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_SCSIMON) -lpthread

clean:
	rm -f $(OBJ_ALL) $(BIN_ALL)

run:
	sudo ./$(RASCSI) -ID1 /home/pi/HARDDISK.HDA -ID6 /home/pi/marathon.iso

.PHONY: Debug
Debug: scsimon
