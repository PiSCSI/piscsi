.DEFAULT_GOAL: all

CC       = gcc
CXX      = g++

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	# Debug CFLAGS
	CFLAGS   = -DDISK_LOG -O0 -g -Wall -DDEBUG
	CXXFLAGS   = -DDISK_LOG -O0 -g -Wall -DDEBUG
	BUILD_TYPE = Debug
else
	# Release CFLAGS
	CFLAGS ?= -O3 -Wall -Werror
	CXXFLAGS ?= -O3 -Wall -Werror
	BUILD_TYPE = Release
endif

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
SCSISHARK = scsishark

USR_LOCAL_BIN = /usr/local/bin
MAN_PAGE_DIR  = /usr/share/man/man1
DOC_DIR = ../../doc

#BIN_ALL = $(RASCSI) $(RASCTL) $(RASDUMP) $(SASIDUMP) $(SCSIMON)
# Temporarily remove the RASDUMP and RASDUMP tools, since they're not needed
# for my specific use case. If you need them - add them back in!
BIN_ALL = $(RASCSI) $(RASCTL) $(SCSISHARK)


SRC_RASCSI = \
	rascsi.cpp \
	scsi.cpp \
	disk.cpp \
	gpiobus.cpp \
	ctapdriver.cpp \
	cfilesystem.cpp \
	filepath.cpp \
	fileio.cpp

SRC_SCSISHARK = \
	scsishark.cpp \
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
OBJ_SCSISHARK  := $(SRC_SCSISHARK:%.cpp=%.o)
#OBJ_ALL := $(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_RASDUMP) $(OBJ_SASIDUMP) $(OBJ_SCSIMON)
OBJ_ALL := $(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_RASDUMP) $(OBJ_SASIDUMP) $(OBJ_SCSISHARK)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@



.DEFAULT_GOAL := all
.PHONY: all ALL docs
all: $(BIN_ALL) docs
ALL: all

docs: $(DOC_DIR)/rascsi_man_page.txt $(DOC_DIR)/rasctl_man_page.txt

$(RASCSI): $(OBJ_RASCSI)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASCSI) -lpthread

$(RASCTL): $(OBJ_RASCTL)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASCTL)

$(RASDUMP): $(OBJ_RASDUMP)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASDUMP)

$(SASIDUMP): $(OBJ_SASIDUMP)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_SASIDUMP)

$(SCSISHARK): $(OBJ_SCSISHARK)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_SCSISHARK) -lpthread

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
