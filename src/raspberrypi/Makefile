.DEFAULT_GOAL: all

ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

CC  = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	# Debug CFLAGS
	CFLAGS   += -DDISK_LOG -O0 -g -Wall -DDEBUG
	CXXFLAGS   += -DDISK_LOG -O0 -g -Wall -DDEBUG
	BUILD_TYPE = Debug
else
	# Release CFLAGS
	CFLAGS += -O3 -Wall -Werror
	CXXFLAGS += -O3 -Wall -Werror
	BUILD_TYPE = Release
endif
CFLAGS += -iquote .
CXXFLAGS += -std=c++14 -iquote .

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

OBJDIR := ./obj
BINDIR := ./bin

#BIN_ALL = $(RASCSI) $(RASCTL) $(RASDUMP) $(SASIDUMP) $(SCSIMON)
# Temporarily remove the RASDUMP and RASDUMP tools, since they're not needed
# for my specific use case. If you need them - add them back in!
BIN_ALL = $(BINDIR)/$(RASCSI) $(BINDIR)/$(RASCTL)


SRC_RASCSI = \
	rascsi.cpp \
	scsi.cpp \
	gpiobus.cpp \
	filepath.cpp \
	fileio.cpp\
#	os.cpp
#	rasctl_command.cpp
#	rascsi_mgr.cpp
#	command_thread.cpp
SRC_RASCSI += $(notdir $(shell find ./controllers -name '*.cpp'))
SRC_RASCSI += $(notdir $(shell find ./devices -name '*.cpp'))

SRC_RASCTL = \
	rasctl.cpp
#	rasctl_command.cpp

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

vpath %.h ./ ./controllers ./devices
vpath %.cpp ./ ./controllers ./devices
vpath %.o ./$(OBJDIR)
vpath ./$(BINDIR)


OBJ_RASCSI := $(SRC_RASCSI:%.cpp=$(OBJDIR)/%.o)
OBJ_RASCTL := $(SRC_RASCTL:%.cpp=$(OBJDIR)/%.o)
OBJ_RASDUMP := $(SRC_RASDUMP:%.cpp=$(OBJDIR)/%.o)
OBJ_SASIDUMP := $(SRC_SASIDUMP:%.cpp=$(OBJDIR)/%.o)
OBJ_SCSIMON  := $(SRC_SCSIMON:%.cpp=$(OBJDIR)/%.o)
#OBJ_ALL := $(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_RASDUMP) $(OBJ_SASIDUMP) $(OBJ_SCSIMON)
OBJ_ALL := $(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_RASDUMP) $(OBJ_SASIDUMP)

$(OBJDIR) $(BINDIR):
	mkdir -p $@

$(OBJDIR)/%.o: %.cpp $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# $(OBJDIR)/%.o: %.c
# 	$(CXX) $(CXXFLAGS) -c $< -o $@

# %.o: %.cpp
# 	$(CXX) $(CXXFLAGS) -c $(OBJDIR)/$< -o $@

.DEFAULT_GOAL := all
.PHONY: all ALL docs
all: $(BIN_ALL) docs
ALL: all

docs: $(DOC_DIR)/rascsi_man_page.txt $(DOC_DIR)/rasctl_man_page.txt

$(BINDIR)/$(RASCSI): $(OBJ_RASCSI) $(BINDIR)
	@echo -- Linking $(RASCSI)
	$(CXX) -o $@ $(OBJ_RASCSI) -lpthread

$(BINDIR)/$(RASCTL): $(OBJ_RASCTL) $(BINDIR)
	@echo -- Linking $(RASCTL)
	$(CXX) -o $@ $(OBJ_RASCTL)

$(RASDUMP): $(OBJ_RASDUMP) $(BINDIR)
	$(CXX) -o $@ $(OBJ_RASDUMP)

$(SASIDUMP): $(OBJ_SASIDUMP) $(BINDIR)
	$(CXX) -o $@ $(OBJ_SASIDUMP)

$(SCSIMON): $(OBJ_SCSIMON) $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_SCSIMON) -lpthread

clean:
	rm -rf $(OBJDIR) $(BINDIR)

run:
	sudo ./$(RASCSI) -ID1 /home/pi/HARDDISK2.hda -ID6 /home/pi/marathon.iso

install: $(MAN_PAGE_DIR)/rascsi.1 $(MAN_PAGE_DIR)/rasctl.1
	sudo cp $(BINDIR)/$(RASCTL) $(USR_LOCAL_BIN)
	sudo cp $(BINDIR)/$(RASCSI) $(USR_LOCAL_BIN)

$(MAN_PAGE_DIR)/%.1 : $(DOC_DIR)/%.1
	sudo cp $< $@

$(DOC_DIR)/%_man_page.txt : $(DOC_DIR)/%.1
	@echo "!!   ------ THIS FILE IS AUTO_GENERATED! DO NOT MANUALLY UPDATE!!!" > $@
	@echo "!!   ------ The native file is $(notdir $<). Re-run 'make docs' after updating\n\n" >> $@
	man -l $< | col -bx >> $@

.PHONY: Debug
Debug: all
