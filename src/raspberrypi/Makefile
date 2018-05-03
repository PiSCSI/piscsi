CC       = gcc
CFLAGS   = -DNDEBUG -O3
CXX      = g++
CXXFLAGS = -DNDEBUG -O3

RASCSI = rascsi
RASCTL = rasctl
RASDUMP = rasdump

BIN_ALL = $(RASCSI) $(RASCTL) $(RASDUMP)

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

OBJ_RASCSI := $(SRC_RASCSI:.cpp=.o)
OBJ_RASCTL := $(SRC_RASCTL:.c=.o)
OBJ_RASDUMP := $(SRC_RASDUMP:.c=.o)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $<

ALL: $(BIN_ALL)

$(RASCSI): $(OBJ_RASCSI) $
	$(CXX) -o $@ $(OBJ_RASCSI) -lpthread

$(RASCTL): $(OBJ_RASCTL) $
	$(CXX) -o $@ $(OBJ_RASCTL)

$(RASDUMP): $(OBJ_RASDUMP) $
	$(CXX) -o $@ $(OBJ_RASDUMP)

clean:
	rm -f *.o $(BIN_ALL)
