.DEFAULT_GOAL: all

## Optional build flags:
##   ARCH=arm : Specify which target platform you're compiling
##              for. This will default to arm, which is typical
##              on a Raspberry Pi.
##   CROSS_COMPILE=arm-linux-gnueabihf- : Specify which compiler
##              toolchain to use. This will default to arm-linux-
##              gnueabihf-, which is typical on a Raspberry Pi.
##              To cross compile on a x86_64 system set these to:
##                 ARM=x86_64 CROSS_COMPILE=x86_64-linux-gnu-cpp
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

CC  = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++

##   DEBUG=1  : A Debug build includes the debugger symbols
##              and disables compiler optimization. Typically,
##              this is only used by developers.
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
CFLAGS += -iquote . -MD -MP 
CXXFLAGS += -std=c++14 -iquote . -MD -MP 

##   CONNECT_TYPE=STANDARD : Specify the type of RaSCSI board type
##              that you are using. The typical options are 
##              STANDARD or FULLSPEC. The default is STANDARD
##              * THIS IS TYPICALLY THE ONLY COMPILE OPTION YOU 
##              * NEED TO SPECIFY
# If its not specified, build for STANDARD configuration
CONNECT_TYPE ?= FULLSPEC

ifdef CONNECT_TYPE
CFLAGS   += -DCONNECT_TYPE_$(CONNECT_TYPE)
CXXFLAGS += -DCONNECT_TYPE_$(CONNECT_TYPE)
endif

RASCSI = rascsi
RASCTL = rasctl
RASDUMP = rasdump
SASIDUMP = sasidump
SCSISHARK = scsishark

SYSTEMD_CONF = /etc/systemd/system/rascsi.service
RSYSLOG_CONF = /etc/rsyslog.d/rascsi.conf
RSYSLOG_LOG  = /var/log/rascsi.log

USR_LOCAL_BIN = /usr/local/bin
MAN_PAGE_DIR  = /usr/share/man/man1
DOC_DIR = ../../doc
OS_FILES = ./os_integration

OBJDIR := ./obj/$(shell echo $(CONNECT_TYPE) | tr '[:upper:]' '[:lower:]')
BINDIR := ./bin/$(shell echo $(CONNECT_TYPE) | tr '[:upper:]' '[:lower:]')

#BIN_ALL = $(RASCSI) $(RASCTL) $(RASDUMP) $(SASIDUMP) $(SCSIMON)
# Temporarily remove the RASDUMP and RASDUMP tools, since they're not needed
# for my specific use case. If you need them - add them back in!
BIN_ALL = $(BINDIR)/$(RASCSI) $(BINDIR)/$(RASCTL) $(BINDIR)/$(RASDUMP)


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
SRC_RASCSI += $(shell find ./controllers -name '*.cpp')
SRC_RASCSI += $(shell find ./devices -name '*.cpp')

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


OBJ_RASCSI := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASCSI:%.cpp=%.o)))
OBJ_RASCTL := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASCTL:%.cpp=%.o)))
OBJ_RASDUMP := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASDUMP:%.cpp=%.o)))
OBJ_SASIDUMP := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SASIDUMP:%.cpp=%.o)))
OBJ_SCSIMON  := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SCSIMON:%.cpp=%.o)))
#OBJ_ALL := $(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_RASDUMP) $(OBJ_SASIDUMP) $(OBJ_SCSIMON)
OBJ_ALL := $(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_RASDUMP) $(OBJ_SASIDUMP) $(OBJ_SCSISHARK)


# The following will include all of the auto-generated dependency files (*.d)
# if they exist. This will trigger a rebuild of a source file if a header changes
ALL_DEPS := $(patsubst %.o,%.d,$(OBJ_RASCSI) $(OBJ_RASCTL))
-include $(ALL_DEPS)

$(OBJDIR) $(BINDIR):
	echo "-- Creating directory $@"
	mkdir -p $@

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

## Build Targets:
##   all      : Rebuild all of the executable files and re-generate 
##              the text versions of the manpages
##   docs     : Re-generate the text versions of the man pages
.DEFAULT_GOAL := all
.PHONY: all ALL docs
all: $(BIN_ALL) docs
ALL: all

docs: $(DOC_DIR)/rascsi_man_page.txt $(DOC_DIR)/rasctl_man_page.txt

$(BINDIR)/$(RASCSI): $(OBJ_RASCSI) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASCSI) -lpthread

$(BINDIR)/$(RASCTL): $(OBJ_RASCTL) $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASCTL)

$(BINDIR)/$(RASDUMP): $(OBJ_RASDUMP) $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASDUMP)

$(RASDUMP): $(OBJ_RASDUMP) $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASDUMP)

$(SASIDUMP): $(OBJ_SASIDUMP) $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_SASIDUMP)

$(SCSIMON): $(OBJ_SCSIMON) $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_SCSIMON) -lpthread

##   clean    : Remove all of the object files, intermediate 
##              compiler files and executable files 
.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(BINDIR)

##   run      : Launches RaSCSI using some pre-defined drive 
##              images. Useful for debugging when you're building 
##              and re-launching over and over.
.PHONY: run
run:
	sudo $(BINDIR)/$(RASCSI) -ID1 /home/pi/HARDDISK2.hda -ID6 /home/pi/marathon.iso

##   install  : Copies all of the man pages to the correct location
##              Copies the binaries to a global install location
##              Configures the Systemd and RSyslog services to auto-run RaSCSI
##              * This target needs to be run with sudo (ex: sudo make install)
##              * Before running this, you need to stop the rascsi service if
##              * it is already running:
##              *   sudo systemctl stop rascsi
##              * After running this, you will need to reboot or run:
##              *   sudo systemctl daemon-reload
##              *   sudo systemctl restart rsyslog
##              *   sudo systemctl enable rascsi
##              *   sudo systemctl start rascsi
.PHONY: install
install: $(MAN_PAGE_DIR)/rascsi.1 $(MAN_PAGE_DIR)/rasctl.1 $(USR_LOCAL_BIN)/$(RASCTL) $(USR_LOCAL_BIN)/$(RASCSI) $(SYSTEMD_CONF) $(RSYSLOG_CONF) $(RSYSLOG_LOG)
	@echo "-- Done installing!"

$(USR_LOCAL_BIN)% : $(BINDIR)/%
	@echo "-- Copying $@"
	cp $< $@

$(MAN_PAGE_DIR)/%.1 : $(DOC_DIR)/%.1
	@echo "-- Copying $@"
	cp $< $@

$(DOC_DIR)/%_man_page.txt : $(DOC_DIR)/%.1
	@echo "!!   ------ THIS FILE IS AUTO_GENERATED! DO NOT MANUALLY UPDATE!!!" > $@
	@echo "!!   ------ The native file is $(notdir $<). Re-run 'make docs' after updating\n\n" >> $@
	man -l $< | col -bx >> $@

$(SYSTEMD_CONF) : $(OS_FILES)/$(notdir $(SYSTEMD_CONF))
	@echo "-- Copying $@"
	cp $< $@

$(RSYSLOG_CONF) : $(OS_FILES)/$(notdir $(RSYSLOG_CONF))
	@echo "-- Copying $@"
	cp $< $@

$(RSYSLOG_LOG) :
	@echo "-- Creating $@"
	touch /var/log/rascsi.log
	chown root:adm /var/log/rascsi.log

##   help     : Lists information about how to use the makefile
# The help rule is based upon the approach from:
#    https://swcarpentry.github.io/make-novice/08-self-doc/index.html 
.PHONY: help
help : Makefile
	@sed -n 's/^##//p' $<

##   Debug    : Same as 'all'. Useful when using a debugger.
.PHONY: Debug
Debug: all
