.DEFAULT_GOAL: all

CC       = gcc
CXX      = g++

# Debug CFLAGS
#CFLAGS   = -DDISK_LOG -O0 -g -Wall
#CXXFLAGS   = -DDISK_LOG -O0 -g -Wall
#CFLAGS   = -O0 -g -Wall
#CXXFLAGS   = -O0 -g -Wall
# Release CFLAGS
CFLAGS = -O3 -Wall
CXXFLAGS = -O3 -Wall



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

USR_LOCAL_BIN = /usr/local/bin
MAN_PAGE_DIR  = /usr/share/man/man1
DOC_DIR = ../../doc

#BIN_ALL = $(RASCSI) $(RASCTL) $(RASDUMP) $(SASIDUMP) $(SCSIMON)
# Temporarily remove the RASDUMP and RASDUMP tools, since they're not needed
# for my specific use case. If you need them - add them back in!
BIN_ALL = $(RASCSI) $(RASCTL)


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

OBJ_RASCSI := $(SRC_RASCSI:%.cpp=%.o)
OBJ_RASCTL := $(SRC_RASCTL:%.cpp=%.o)
OBJ_RASDUMP := $(SRC_RASDUMP:%.cpp=%.o)
OBJ_SASIDUMP := $(SRC_SASIDUMP:%.cpp=%.o)
OBJ_SCSIMON  := $(SRC_SCSIMON:%.cpp=%.o)
#OBJ_ALL := $(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_RASDUMP) $(OBJ_SASIDUMP) $(OBJ_SCSIMON)
OBJ_ALL := $(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_RASDUMP) $(OBJ_SASIDUMP)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

docs: $(DOC_DIR)/rascsi_man_page.txt $(DOC_DIR)/rasctl_man_page.txt

ALL: $(BIN_ALL) docs
all: $(BIN_ALL) docs

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
	sudo ./$(RASCSI) -ID1 /home/pi/HARDDISK2.hda -ID6 /home/pi/marathon.iso

install: $(MAN_PAGE_DIR)/rascsi.1 $(MAN_PAGE_DIR)/rasctl.1
	sudo cp $(RASCTL) $(USR_LOCAL_BIN)
	sudo cp $(RASCSI) $(USR_LOCAL_BIN)

$(MAN_PAGE_DIR)/%.1 : $(DOC_DIR)/%.1
	sudo cp $< $@

$(DOC_DIR)/%_man_page.txt : $(DOC_DIR)/%.1
	@echo "!!   ------ THIS FILE IS AUTO_GENERATED! DO NOT MANUALLY UPDATE!!!" > $@
	@echo "!!   ------ The native file is $(notdir $<). Re-run 'make docs' after updating\n\n" >> $@
	man -l $< | col -bx >> $@

.PHONY: Debug
Debug: all
