.DEFAULT_GOAL: all

## Optional build flags:
##   CROSS_COMPILE : Specify which compiler toolchain to use.
##                   To cross compile set this accordingly, e.g. to:
##                   arm-linux-gnueabihf-
CROSS_COMPILE =

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++

##   DEBUG=1  : A Debug build includes the debugger symbols
##              and disables compiler optimization. Typically,
##              this is only used by developers.
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	# Debug CFLAGS
	CFLAGS += -O0 -g -Wall -DDEBUG
	CXXFLAGS += -O0 -g -Wall -DDEBUG
	BUILD_TYPE = Debug
else
	# Release CFLAGS
	CFLAGS += -O3 -Wall -Werror -DNDEBUG
	CXXFLAGS += -O3 -Wall -Werror -DNDEBUG
	BUILD_TYPE = Release
endif
CFLAGS += -iquote . -D_FILE_OFFSET_BITS=64 -MD -MP
CXXFLAGS += -std=c++17 -Wno-psabi -iquote . -D_FILE_OFFSET_BITS=64 -MD -MP

## EXTRA_FLAGS : Can be used to pass special purpose flags
CFLAGS += $(EXTRA_FLAGS)
CXXFLAGS += $(EXTRA_FLAGS)

# If we're using GCC version 10 or later, we need to add the FMT_HEADER_ONLY definition
GCCVERSION10 := $(shell expr `gcc -dumpversion` \>= 10)

ifeq "$(GCCVERSION10)" "1"
	CFLAGS += -DFMT_HEADER_ONLY
	CXXFLAGS += -DFMT_HEADER_ONLY
endif



##   CONNECT_TYPE=FULLSPEC : Specify the type of RaSCSI board type
##              that you are using. The typical options are 
##              STANDARD or FULLSPEC. The default is FULLSPEC
##              * THIS IS TYPICALLY THE ONLY COMPILE OPTION YOU 
##              * NEED TO SPECIFY
# If its not specified, build for FULLSPEC configuration
CONNECT_TYPE ?= FULLSPEC

ifdef CONNECT_TYPE
CFLAGS += -DCONNECT_TYPE_$(CONNECT_TYPE)
CXXFLAGS += -DCONNECT_TYPE_$(CONNECT_TYPE)
endif

RASCSI = rascsi
RASCTL = rasctl
RASDUMP = rasdump
SASIDUMP = sasidump
SCSIMON = scsimon

SYSTEMD_CONF = /etc/systemd/system/rascsi.service
RSYSLOG_CONF = /etc/rsyslog.d/rascsi.conf
RSYSLOG_LOG  = /var/log/rascsi.log

USR_LOCAL_BIN = /usr/local/bin
MAN_PAGE_DIR  = /usr/local/man/man1
DOC_DIR = ../../doc
OS_FILES = ./os_integration

OBJDIR := ./obj/$(shell echo $(CONNECT_TYPE) | tr '[:upper:]' '[:lower:]')
BINDIR := ./bin/$(shell echo $(CONNECT_TYPE) | tr '[:upper:]' '[:lower:]')

BIN_ALL = \
	$(BINDIR)/$(RASCSI) \
	$(BINDIR)/$(RASCTL) \
	$(BINDIR)/$(SCSIMON) \
	$(BINDIR)/$(RASDUMP) \
	$(BINDIR)/$(SASIDUMP)

SRC_PROTOC = \
	rascsi_interface.proto

SRC_PROTOBUF = \
	rascsi_interface.pb.cpp

SRC_RASCSI = \
	rascsi.cpp \
	scsi.cpp \
	gpiobus.cpp \
	filepath.cpp \
	fileio.cpp\
	rascsi_version.cpp \
	rascsi_image.cpp \
	rascsi_response.cpp \
	rasutil.cpp \
	protobuf_util.cpp \
	localizer.cpp
SRC_RASCSI += $(shell find ./controllers -name '*.cpp')
SRC_RASCSI += $(shell find ./devices -name '*.cpp')
SRC_RASCSI += $(SRC_PROTOBUF)

SRC_SCSIMON = \
	scsimon.cpp \
	scsi.cpp \
	gpiobus.cpp \
	filepath.cpp \
	fileio.cpp \
	rascsi_version.cpp

SRC_RASCTL = \
	rasctl.cpp\
	rasctl_commands.cpp \
	rasctl_display.cpp \
	rascsi_version.cpp \
	rasutil.cpp \
	protobuf_util.cpp \
	localizer.cpp
SRC_RASCTL += $(SRC_PROTOBUF)

SRC_RASDUMP = \
	rasdump.cpp \
	scsi.cpp \
	gpiobus.cpp \
	filepath.cpp \
	fileio.cpp\
	rascsi_version.cpp

SRC_SASIDUMP = \
	sasidump.cpp \
	scsi.cpp \
	gpiobus.cpp \
	filepath.cpp \
	fileio.cpp\
	rascsi_version.cpp

vpath %.h ./ ./controllers ./devices
vpath %.cpp ./ ./controllers ./devices
vpath %.o ./$(OBJDIR)
vpath ./$(BINDIR)


OBJ_RASCSI := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASCSI:%.cpp=%.o)))
OBJ_RASCTL := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASCTL:%.cpp=%.o)))
OBJ_RASDUMP := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASDUMP:%.cpp=%.o)))
OBJ_SASIDUMP := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SASIDUMP:%.cpp=%.o)))
OBJ_SCSIMON := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SCSIMON:%.cpp=%.o)))

GEN_PROTOBUF := $(SRC_PROTOBUF) rascsi_interface.pb.h


# The following will include all of the auto-generated dependency files (*.d)
# if they exist. This will trigger a rebuild of a source file if a header changes
ALL_DEPS := $(patsubst %.o,%.d,$(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_SCSIMON))
-include $(ALL_DEPS)

$(OBJDIR) $(BINDIR):
	echo "-- Creating directory $@"
	mkdir -p $@

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_PROTOBUF): $(SRC_PROTOC)
	echo "-- Generating protobuf-based source files"
	protoc --cpp_out=. $(SRC_PROTOC)
	mv rascsi_interface.pb.cc $@

## Build Targets:
##   all      : Rebuild all of the executable files and re-generate 
##              the text versions of the manpages
##   docs     : Re-generate the text versions of the man pages
.DEFAULT_GOAL := all
.PHONY: all ALL docs
all: $(BIN_ALL) docs
ALL: all

docs: $(DOC_DIR)/rascsi_man_page.txt $(DOC_DIR)/rasctl_man_page.txt $(DOC_DIR)/scsimon_man_page.txt

$(BINDIR)/$(RASCSI): $(SRC_PROTOBUF) $(OBJ_RASCSI) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASCSI) -lpthread -lz -lpcap -lprotobuf -lstdc++fs

$(BINDIR)/$(RASCTL): $(SRC_PROTOBUF) $(OBJ_RASCTL) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASCTL) -lpthread -lprotobuf -lstdc++fs

$(BINDIR)/$(RASDUMP): $(OBJ_RASDUMP) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASDUMP)

$(BINDIR)/$(SASIDUMP): $(OBJ_SASIDUMP) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_SASIDUMP)

$(BINDIR)/$(SCSIMON): $(OBJ_SCSIMON) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_SCSIMON) -lpthread

##   clean    : Remove all of the object files, intermediate 
##              compiler files and executable files 
.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(BINDIR) $(GEN_PROTOBUF)

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
install: \
	$(MAN_PAGE_DIR)/rascsi.1 \
	$(MAN_PAGE_DIR)/rasctl.1 \
	$(MAN_PAGE_DIR)/scsimon.1 \
	$(MAN_PAGE_DIR)/rasdump.1 \
	$(MAN_PAGE_DIR)/sasidump.1 \
	$(USR_LOCAL_BIN)/$(RASCTL) \
	$(USR_LOCAL_BIN)/$(RASCSI) \
	$(USR_LOCAL_BIN)/$(SCSIMON) \
	$(USR_LOCAL_BIN)/$(RASDUMP) \
	$(USR_LOCAL_BIN)/$(SASIDUMP) \
	$(SYSTEMD_CONF) \
	$(RSYSLOG_CONF) \
	$(RSYSLOG_LOG)
	@echo "-- Done installing!"

$(USR_LOCAL_BIN)% : $(BINDIR)/%
	@echo "-- Copying $@"
	cp $< $@

$(MAN_PAGE_DIR)/%.1 : $(DOC_DIR)/%.1 | $(MAN_PAGE_DIR)/
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

$(MAN_PAGE_DIR)/:
	echo "-- Creating directory $@"
	mkdir -p $@

##   help     : Lists information about how to use the makefile
# The help rule is based upon the approach from:
#    https://swcarpentry.github.io/make-novice/08-self-doc/index.html 
.PHONY: help
help : Makefile
	@sed -n 's/^##//p' $<

##   Debug    : Same as 'all'. Useful when using a debugger.
.PHONY: Debug
Debug: all

