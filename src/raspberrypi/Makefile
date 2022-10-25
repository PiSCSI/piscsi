.DEFAULT_GOAL: all

## Optional build flags:
##   CROSS_COMPILE : Specify which compiler toolchain to use.
##                   To cross compile set this accordingly, e.g. to:
##                   arm-linux-gnueabihf-
CROSS_COMPILE = 

CXX = $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)ar
RANLIB = $(CROSS_COMPILE)ranlib

##   DEBUG=1  : A Debug build includes the debugger symbols
##              and disables compiler optimization. Typically,
##              this is only used by developers.
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	# Debug compiler flags
	CXXFLAGS += -O0 -g -Wall -Wextra -DDEBUG
	BUILD_TYPE = Debug
else
	# Release compiler flags
	CXXFLAGS += -O3 -Wall -Werror -Wextra -DNDEBUG
	BUILD_TYPE = Release
endif
ifeq ("$(shell uname -s)","Linux")
	# -Wno-psabi might not work on non-Linux platforms
	CXXFLAGS += -Wno-psabi
endif

# Depending on the GCC version the compilation flags differ
GCCVERSION10 := $(shell expr `$(CXX) -dumpversion` \>= 10)

CXXFLAGS += -std=c++17 -iquote . -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -MD -MP -I../BPI-WiringPi2/wiringPi

## EXTRA_FLAGS : Can be used to pass special purpose flags
CXXFLAGS += $(EXTRA_FLAGS)

ifeq "$(GCCVERSION10)" "1"
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
CXXFLAGS += -DCONNECT_TYPE_$(CONNECT_TYPE)
endif

RASCSI = rascsi
RASCTL = rasctl
RASDUMP = rasdump
SCSIMON = scsimon
RASCSI_TEST = rascsi_test
REGISTERS_TEST = registers
GPIO_TEST = gpiotest

SYSTEMD_CONF = /etc/systemd/system/rascsi.service
RSYSLOG_CONF = /etc/rsyslog.d/rascsi.conf
RSYSLOG_LOG  = /var/log/rascsi.log

USR_LOCAL_BIN = /usr/local/bin
MAN_PAGE_DIR  = /usr/local/man/man1
DOC_DIR = ../../doc
COVERAGE_DIR = ./coverage
COVERAGE_FILE = rascsi.dat
OS_FILES = ./os_integration

OBJDIR := ./obj/$(shell echo $(CONNECT_TYPE) | tr '[:upper:]' '[:lower:]')
BINDIR := ./bin/$(shell echo $(CONNECT_TYPE) | tr '[:upper:]' '[:lower:]')

BIN_ALL = \
	$(BINDIR)/$(RASCSI) \
	$(BINDIR)/$(RASCTL) \
	$(BINDIR)/$(SCSIMON) \
	$(BINDIR)/$(RASDUMP)

SRC_PROTOC = \
	rascsi_interface.proto

SRC_PROTOBUF = \
	rascsi_interface.pb.cpp

SRC_SHARED = \
	rascsi_version.cpp \
	rasutil.cpp \
	protobuf_util.cpp \
	protobuf_serializer.cpp

SRC_RASCSI_CORE = \
	bus.cpp \
	filepath.cpp \
	fileio.cpp
SRC_RASCSI_CORE += $(shell find ./rascsi -name '*.cpp')
SRC_RASCSI_CORE += $(shell find ./controllers -name '*.cpp')
SRC_RASCSI_CORE += $(shell find ./devices -name '*.cpp')
SRC_RASCSI_CORE += $(shell find ./hal -name '*.cpp')
SRC_RASCSI_CORE += $(shell find ./hal/boards -name '*.cpp')

SRC_RASCSI = rascsi.cpp

SRC_SCSIMON = \
	scsimon.cpp \
	bus.cpp \
	rascsi_version.cpp
SRC_SCSIMON += $(shell find ./monitor -name '*.cpp')
SRC_SCSIMON += $(shell find ./hal -name '*.cpp')

SRC_RASCTL_CORE = $(shell find ./rasctl -name '*.cpp')

SRC_RASCTL = rasctl.cpp

SRC_RASDUMP = \
	rasdump.cpp \
	bus.cpp \
	filepath.cpp \
	fileio.cpp \
	rascsi_version.cpp
SRC_RASDUMP += $(shell find ./hal -name '*.cpp')

SRC_RASCSI_TEST = $(shell find ./test -name '*.cpp')

SRC_GPIO_TEST = \
	scsi.cpp \
	gpiotest.cpp
SRC_GPIO_TEST += $(shell find ./hal -name '*.cpp')

vpath %.h ./ ./controllers ./devices ./monitor ./hal ./hal/boards ./rascsi ./rasctl ../BPI-WiringPi2/wiringPi/
vpath %.cpp ./ ./controllers ./devices ./monitor ./test ./hal ./hal/boards ./rascsi ./rasctl
vpath %.o ./$(OBJDIR)
vpath ./$(BINDIR)


OBJ_RASCSI_CORE := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASCSI_CORE:%.cpp=%.o)))
OBJ_RASCSI := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASCSI:%.cpp=%.o)))
OBJ_RASCTL_CORE := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASCTL_CORE:%.cpp=%.o)))
OBJ_RASCTL := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASCTL:%.cpp=%.o)))
OBJ_RASDUMP := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASDUMP:%.cpp=%.o)))
OBJ_SCSIMON := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SCSIMON:%.cpp=%.o)))
OBJ_RASCSI_TEST := $(addprefix $(OBJDIR)/,$(notdir $(SRC_RASCSI_TEST:%.cpp=%.o)))
OBJ_SHARED := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SHARED:%.cpp=%.o)))
OBJ_PROTOBUF := $(addprefix $(OBJDIR)/,$(notdir $(SRC_PROTOBUF:%.cpp=%.o)))
OBJ_GPIO_TEST := $(addprefix $(OBJDIR)/,$(notdir $(SRC_GPIO_TEST:%.cpp=%.o)))

GEN_PROTOBUF := $(SRC_PROTOBUF) rascsi_interface.pb.h

LIB_BPI_WIRINGPI := $(OBJDIR)/libwiringPi.a

# The following will include all of the auto-generated dependency files (*.d)
# if they exist. This will trigger a rebuild of a source file if a header changes
ALL_DEPS := $(patsubst %.o,%.d,$(OBJ_RASCSI_CORE) $(OBJ_RASCTL_CORE) $(OBJ_RASCSI) $(OBJ_RASCTL) $(OBJ_SCSIMON) $(OBJ_RASCSI_TEST))
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
##   test     : Build and run unit tests
##   coverage : Build and run unit tests and create coverage SonarQube files.
##   lcov     : Build and run unit tests and create coverage HTML files.
##              Note that you have to run 'make clean' before switching
##              between coverage and non-coverage builds.
.DEFAULT_GOAL := all
.PHONY: all ALL docs test coverage lcov
all: $(BIN_ALL) docs

test: $(BINDIR)/$(RASCSI_TEST)
	$(BINDIR)/$(RASCSI_TEST)

coverage: CXXFLAGS += --coverage
coverage: test

lcov: CXXFLAGS += --coverage
lcov: test
	lcov -q -c -d . --include '*/raspberrypi/*' -o $(COVERAGE_FILE) --exclude '*/test/*' --exclude '*/interfaces/*' --exclude '*/rascsi_interface.pb*'
	genhtml -q -o $(COVERAGE_DIR) --legend $(COVERAGE_FILE)

docs: $(DOC_DIR)/rascsi_man_page.txt $(DOC_DIR)/rasctl_man_page.txt $(DOC_DIR)/scsimon_man_page.txt

$(SRC_SHARED) $(SRC_RASCSI_CORE) $(SRC_RASCTL_CORE): $(OBJ_PROTOBUF)

$(BINDIR)/$(RASCSI): $(SRC_PROTOBUF) $(OBJ_RASCSI_CORE) $(OBJ_RASCSI) $(OBJ_SHARED) $(OBJ_PROTOBUF) $(LIB_BPI_WIRINGPI) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASCSI_CORE) $(OBJ_RASCSI) $(OBJ_SHARED) $(OBJ_PROTOBUF) $(LIB_BPI_WIRINGPI)  -lpthread -lpcap -lprotobuf -lstdc++fs

$(BINDIR)/$(RASCTL): $(SRC_PROTOBUF) $(OBJ_RASCTL_CORE) $(OBJ_RASCTL) $(OBJ_SHARED) $(OBJ_PROTOBUF) $(LIB_BPI_WIRINGPI) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASCTL_CORE) $(OBJ_RASCTL) $(OBJ_SHARED) $(OBJ_PROTOBUF) -lpthread -lprotobuf

$(BINDIR)/$(RASDUMP): $(OBJ_RASDUMP) $(LIB_BPI_WIRINGPI) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASDUMP) $(LIB_BPI_WIRINGPI) -lpthread 

$(BINDIR)/$(SCSIMON): $(OBJ_SCSIMON) $(LIB_BPI_WIRINGPI) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_SCSIMON) $(LIB_BPI_WIRINGPI) -lpthread

$(BINDIR)/$(RASCSI_TEST): $(SRC_PROTOBUF) $(OBJ_RASCSI_CORE) $(OBJ_RASCTL_CORE) $(OBJ_RASCSI_TEST) $(OBJ_RASCTL_TEST) $(OBJ_SHARED) $(OBJ_PROTOBUF) $(LIB_BPI_WIRINGPI) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RASCSI_CORE) $(OBJ_RASCTL_CORE) $(OBJ_RASCSI_TEST) $(OBJ_SHARED) $(OBJ_PROTOBUF) $(LIB_BPI_WIRINGPI) -lpthread -lpcap -lprotobuf -lstdc++fs -lgmock -lgtest

$(BINDIR)/$(GPIO_TEST): $(OBJ_GPIO_TEST) $(LIB_BPI_WIRINGPI) | $(BINDIR)
	echo  $(OBJ_GPIO_TEST)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_GPIO_TEST) $(LIB_BPI_WIRINGPI) -lpthread

# Phony rules for building individual utilities
.PHONY: $(RASCSI) $(RASCTL) $(RASDUMP) $(SCSIMON) $(GPIO_TEST)
$(RASCSI)  : $(BINDIR)/$(RASCSI) 
$(RASCTL)  : $(BINDIR)/$(RASCTL) 
$(RASDUMP) : $(BINDIR)/$(RASDUMP) 
$(SCSIMON) : $(BINDIR)/$(SCSIMON)
$(GPIO_TEST) : $(BINDIR)/$(GPIO_TEST)


##   clean    : Remove all of the object files, intermediate 
##              compiler files and executable files 
.PHONY: clean
clean:
	make -j1 -C ../BPI-WiringPi2/wiringPi/ clean
	rm -rf $(OBJDIR) $(BINDIR) $(GEN_PROTOBUF) $(COVERAGE_DIR) $(COVERAGE_FILE)

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
	$(USR_LOCAL_BIN)/$(RASCTL) \
	$(USR_LOCAL_BIN)/$(RASCSI) \
	$(USR_LOCAL_BIN)/$(SCSIMON) \
	$(USR_LOCAL_BIN)/$(RASDUMP) \
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

$(LIB_BPI_WIRINGPI) : | $(OBJDIR)
	make -j1 -C ../BPI-WiringPi2/wiringPi/ static CC=$(CC) AR=$(AR) RANLIB=$(RANLIB)
	cp ../BPI-WiringPi2/wiringPi/libwiringPi.a $@



##   help     : Lists information about how to use the makefile
# The help rule is based upon the approach from:
#    https://swcarpentry.github.io/make-novice/08-self-doc/index.html 
.PHONY: help
help : Makefile
	@sed -n 's/^##//p' $<

##   Debug    : Same as 'all'. Useful when using a debugger.
.PHONY: Debug
Debug: all

