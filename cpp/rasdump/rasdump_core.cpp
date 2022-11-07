//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ HDD dump utility (initiator mode) ]
//
//---------------------------------------------------------------------------

#include <cerrno>
#include <csignal>
#include <unistd.h>
#include "rasdump/rasdump_fileio.h"
#include "rasdump/rasdump_core.h"
#include "hal/gpiobus.h"
#include "hal/gpiobus_factory.h"
#include "hal/systimer.h"
#include "rascsi_version.h"
#include <cstring>
#include <iostream>
#include <array>

using namespace std;

//---------------------------------------------------------------------------
//
//	Constant Declaration
//
//---------------------------------------------------------------------------
static const int BUFSIZE = 1024 * 64;			// Buffer size of about 64KB

//---------------------------------------------------------------------------
//
//	Variable Declaration
//
//---------------------------------------------------------------------------
unique_ptr<GPIOBUS> bus;		// GPIO Bus
int targetid;					// Target ID
int boardid;					// Board ID (own ID)
string hdsfile;					// HDS file
bool restore;					// Restore flag
array<uint8_t, BUFSIZE> buffer;	// Work Buffer
int result;						// Result Code

//---------------------------------------------------------------------------
//
//	Cleanup() Function declaration
//
//---------------------------------------------------------------------------
void Cleanup();

//---------------------------------------------------------------------------
//
//	Signal processing
//
//---------------------------------------------------------------------------
void KillHandler(int)
{
	// Stop running
	Cleanup();
	exit(0);
}

//---------------------------------------------------------------------------
//
//	Banner Output
//
//---------------------------------------------------------------------------
bool RasDump::Banner(const vector<char *>& args)
{
	printf("RaSCSI hard disk dump utility ");
	printf("version %s (%s, %s)\n",
		rascsi_get_version_string().c_str(),
		__DATE__,
		__TIME__);

	if (args.size() < 2 || strcmp(args[1], "-h") == 0) {
		printf("Usage: %s -i ID [-b BID] -f FILE [-r]\n", args[0]);
		printf(" ID is target device SCSI ID {0|1|2|3|4|5|6|7}.\n");
		printf(" BID is rascsi board SCSI ID {0|1|2|3|4|5|6|7}. Default is 7.\n");
		printf(" FILE is HDS file path.\n");
		printf(" -r is restore operation.\n");
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
bool RasDump::Init()
{
	// Interrupt handler setting
	if (signal(SIGINT, KillHandler) == SIG_ERR) {
		return false;
	}
	if (signal(SIGHUP, KillHandler) == SIG_ERR) {
		return false;
	}
	if (signal(SIGTERM, KillHandler) == SIG_ERR) {
		return false;
	}

	bus = GPIOBUS_Factory::Create(BUS::mode_e::INITIATOR, board_type::rascsi_board_type_e::BOARD_TYPE_FULLSPEC);

	// Work Intitialization
	targetid = -1;
	boardid = 7;
	restore = false;

	return true;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void Cleanup()
{
	// Cleanup the bus
	bus->Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void RasDump::Reset()
{
	// Reset the bus signal line
	bus->Reset();
}

//---------------------------------------------------------------------------
//
//	Argument processing
//
//---------------------------------------------------------------------------
bool RasDump::ParseArguments(const vector<char *>& args)
{
	int opt;
	const char *file = nullptr;

	// Argument Parsing
	opterr = 0;
	while ((opt = getopt(args.size(), args.data(), "i:b:f:r")) != -1) {
		switch (opt) {
			case 'i':
				targetid = optarg[0] - '0';
				break;

			case 'b':
				boardid = optarg[0] - '0';
				break;

			case 'f':
				file = optarg;
				break;

			case 'r':
				restore = true;
				break;

			default:
				break;
		}
	}

	// TARGET ID check
	if (targetid < 0 || targetid > 7) {
		fprintf(stderr,
			"Error : Invalid target id range\n");
		return false;
	}

	// BOARD ID check
	if (boardid < 0 || boardid > 7) {
		fprintf(stderr,
			"Error : Invalid board id range\n");
		return false;
	}

	// Target and Board ID duplication check
	if (targetid == boardid) {
		fprintf(stderr,
			"Error : Invalid target or board id\n");
		return false;
	}

	// File Check
	if (!file) {
		fprintf(stderr,
			"Error : Invalid file path\n");
		return false;
	}

	hdsfile = file;

	return true;
}

//---------------------------------------------------------------------------
//
//	Wait Phase
//
//---------------------------------------------------------------------------
bool RasDump::WaitPhase(BUS::phase_t phase)
{
	// Timeout (3000ms)
	const uint32_t now = SysTimer::GetTimerLow();
	while ((SysTimer::GetTimerLow() - now) < 3 * 1000 * 1000) {
		bus->Acquire();
		if (bus->GetREQ() && bus->GetPhase() == phase) {
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//
//	Bus Free Phase
//
//---------------------------------------------------------------------------
void RasDump::BusFree()
{
	// Bus Reset
	bus->Reset();
}

//---------------------------------------------------------------------------
//
//	Selection Phase
//
//---------------------------------------------------------------------------
bool RasDump::Selection(int id)
{
	// ID setting and SEL assert
	uint8_t data = 1 << boardid;
	data |= (1 << id);
	bus->SetDAT(data);
	bus->SetSEL(true);

	// wait for busy
	int count = 10000;
	do {
		// Wait 20 microseconds
		const timespec ts = { .tv_sec = 0, .tv_nsec = 20 * 1000};
		nanosleep(&ts, nullptr);
		bus->Acquire();
		if (bus->GetBSY()) {
			break;
		}
	} while (count--);

	// SEL negate
	bus->SetSEL(false);

	// Success if the target is busy
	return bus->GetBSY();
}

//---------------------------------------------------------------------------
//
//	Command Phase
//
//---------------------------------------------------------------------------
bool RasDump::Command(uint8_t *buf, int length)
{
	// Waiting for Phase
	if (!WaitPhase(BUS::phase_t::command)) {
		return false;
	}

	// Send Command
	const int count = bus->SendHandShake(buf, length, BUS::SEND_NO_DELAY);

	// Success if the transmission result is the same as the number
	// of requests
	if (count == length) {
		return true;
	}

	// Return error
	return false;
}

//---------------------------------------------------------------------------
//
//	Data in phase
//
//---------------------------------------------------------------------------
int RasDump::DataIn(uint8_t *buf, int length)
{
	// Wait for phase
	if (!WaitPhase(BUS::phase_t::datain)) {
		return -1;
	}

	// Data reception
	return bus->ReceiveHandShake(buf, length);
}

//---------------------------------------------------------------------------
//
//	Data out phase
//
//---------------------------------------------------------------------------
int RasDump::DataOut(uint8_t *buf, int length)
{
	// Wait for phase
	if (!WaitPhase(BUS::phase_t::dataout)) {
		return -1;
	}

	// Data transmission
	return bus->SendHandShake(buf, length, BUS::SEND_NO_DELAY);
}

//---------------------------------------------------------------------------
//
//	Status Phase
//
//---------------------------------------------------------------------------
int RasDump::Status()
{
	uint8_t buf[256];

	// Wait for phase
	if (!WaitPhase(BUS::phase_t::status)) {
		return -2;
	}

	// Data reception
	if (bus->ReceiveHandShake(buf, 1) == 1) {
		return (int)buf[0];
	}

	// Return error
	return -1;
}

//---------------------------------------------------------------------------
//
//	Message in phase
//
//---------------------------------------------------------------------------
int RasDump::MessageIn()
{
	uint8_t buf[256];

	// Wait for phase
	if (!WaitPhase(BUS::phase_t::msgin)) {
		return -2;
	}

	// Data reception
	if (bus->ReceiveHandShake(buf, 1) == 1) {
		return (int)buf[0];
	}

	// Return error
	return -1;
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
int RasDump::TestUnitReady(int id)
{
	array<uint8_t, 256> cmd = {};

	// Result code initialization
	result = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	cmd[0] = 0x00;
	if (!Command(cmd.data(), 6)) {
		result = -2;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// Bus free
	BusFree();

	return result;
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
int RasDump::RequestSense(int id, uint8_t *buf)
{
	array<uint8_t, 256> cmd = {};

	// Result code initialization
	result = 0;
	int count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	cmd[0] = 0x03;
	cmd[4] = 0xff;
	if (!Command(cmd.data(), 6)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	memset(buf, 0x00, 256);
	count = DataIn(buf, 256);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// Bus Free
	BusFree();

	// Returns the number of transfers if successful
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	MODE SENSE
//
//---------------------------------------------------------------------------
int RasDump::ModeSense(int id, uint8_t *buf)
{
	array<uint8_t, 256> cmd = {};

	// Result code initialization
	result = 0;
	int count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	cmd[0] = 0x1a;
	cmd[2] = 0x3f;
	cmd[4] = 0xff;
	if (!Command(cmd.data(), 6)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	memset(buf, 0x00, 256);
	count = DataIn(buf, 256);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// Bus free
	BusFree();

	// Returns the number of transfers if successful
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int RasDump::Inquiry(int id, uint8_t *buf)
{
	array<uint8_t, 256> cmd = {};

	// Result code initialization
	result = 0;
	int count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	cmd[0] = 0x12;
	cmd[4] = 0xff;
	if (!Command(cmd.data(), 6)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	memset(buf, 0x00, 256);
	count = DataIn(buf, 256);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// Bus free
	BusFree();

	// Returns the number of transfers if successful
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	READ CAPACITY
//
//---------------------------------------------------------------------------
int RasDump::ReadCapacity(int id, uint8_t *buf)
{
	array<uint8_t, 256> cmd = {};

	// Result code initialization
	result = 0;
	int count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	cmd[0] = 0x25;
	if (!Command(cmd.data(), 10)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	memset(buf, 0x00, 8);
	count = DataIn(buf, 8);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// Bus free
	BusFree();

	// Returns the number of transfers if successful
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	READ10
//
//---------------------------------------------------------------------------
int RasDump::Read10(int id, uint32_t bstart, uint32_t blength, uint32_t length, uint8_t *buf)
{
	array<uint8_t, 256> cmd = {};

	// Result code initialization
	result = 0;
	int count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	cmd[0] = 0x28;
	cmd[2] = (uint8_t)(bstart >> 24);
	cmd[3] = (uint8_t)(bstart >> 16);
	cmd[4] = (uint8_t)(bstart >> 8);
	cmd[5] = (uint8_t)bstart;
	cmd[7] = (uint8_t)(blength >> 8);
	cmd[8] = (uint8_t)blength;
	if (!Command(cmd.data(), 10)) {
		result = -2;
		goto exit;
	}

	// DATAIN
	count = DataIn(buf, length);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// Bus free
	BusFree();

	// Returns the number of transfers if successful
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	WRITE10
//
//---------------------------------------------------------------------------
int RasDump::Write10(int id, uint32_t bstart, uint32_t blength, uint32_t length, uint8_t *buf)
{
	array<uint8_t, 256> cmd = {};

	// Result code initialization
	result = 0;
	int count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	cmd[0] = 0x2a;
	cmd[2] = (uint8_t)(bstart >> 24);
	cmd[3] = (uint8_t)(bstart >> 16);
	cmd[4] = (uint8_t)(bstart >> 8);
	cmd[5] = (uint8_t)bstart;
	cmd[7] = (uint8_t)(blength >> 8);
	cmd[8] = (uint8_t)blength;
	if (!Command(cmd.data(), 10)) {
		result = -2;
		goto exit;
	}

	// DATAOUT
	count = DataOut(buf, length);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	// STATUS
	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	// MESSAGE IN
	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	// Bus free
	BusFree();

	// Returns the number of transfers if successful
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	Main process
//
//---------------------------------------------------------------------------
int RasDump::run(const vector<char *>& args)
{
	int i;
	char str[32];
	uint32_t bsiz;
	uint32_t bnum;
	uint32_t duni;
	uint32_t dsiz;
	uint32_t dnum;
	Fileio fio;
	Fileio::OpenMode omode;
	off_t size;

	// Banner output
	if (!Banner(args)) {
		exit(0);
	}

	// Initialization
	if (!Init()) {
		fprintf(stderr, "Error : Initializing. Are you root?\n");

		// Probably not root
		exit(EPERM);
	}

	// Prase Argument
	if (!ParseArguments(args)) {
		// Cleanup
		Cleanup();

		// Exit with invalid argument error
		exit(EINVAL);
	}

#ifndef USE_SEL_EVENT_ENABLE
	cerr << "Error: No RaSCSI hardware support" << endl;
	exit(EXIT_FAILURE);
#endif

	// Reset the SCSI bus
	Reset();

	// File Open
	if (restore) {
		omode = Fileio::OpenMode::ReadOnly;
	} else {
		omode = Fileio::OpenMode::WriteOnly;
	}
	if (!fio.Open(hdsfile.c_str(), omode)) {
		fprintf(stderr, "Error : Can't open hds file\n");

		// Cleanup
		Cleanup();
		exit(EPERM);
	}

	// Bus free
	BusFree();

	// Assert reset signal
	bus->SetRST(true);
	// Wait 1 ms
	const timespec ts = { .tv_sec = 0, .tv_nsec = 1000 * 1000};
	nanosleep(&ts, nullptr);
	bus->SetRST(false);

	// Start dump
	printf("TARGET ID               : %d\n", targetid);
	printf("BOARD ID                : %d\n", boardid);

	// TEST UNIT READY
	int count = TestUnitReady(targetid);
	if (count < 0) {
		fprintf(stderr, "TEST UNIT READY ERROR %d\n", count);
		goto cleanup_exit;
	}

	// REQUEST SENSE(for CHECK CONDITION)
	count = RequestSense(targetid, buffer.data());
	if (count < 0) {
		fprintf(stderr, "REQUEST SENSE ERROR %d\n", count);
		goto cleanup_exit;
	}

	// INQUIRY
	count = Inquiry(targetid, buffer.data());
	if (count < 0) {
		fprintf(stderr, "INQUIRY ERROR %d\n", count);
		goto cleanup_exit;
	}

	// Display INQUIRY information
	memset(str, 0x00, sizeof(str));
	memcpy(str, &buffer[8], 8);
	printf("Vendor                  : %s\n", str);
	memset(str, 0x00, sizeof(str));
	memcpy(str, &buffer[16], 16);
	printf("Product                 : %s\n", str);
	memset(str, 0x00, sizeof(str));
	memcpy(str, &buffer[32], 4);
	printf("Revison                 : %s\n", str);

	// Get drive capacity
	count = ReadCapacity(targetid, buffer.data());
	if (count < 0) {
		fprintf(stderr, "READ CAPACITY ERROR %d\n", count);
		goto cleanup_exit;
	}

	// Display block size and number of blocks
	bsiz =
		(buffer[4] << 24) | (buffer[5] << 16) |
		(buffer[6] << 8) | buffer[7];
	bnum =
		(buffer[0] << 24) | (buffer[1] << 16) |
		(buffer[2] << 8) | buffer[3];
	bnum++;
	printf("Number of blocks        : %d Blocks\n", (int)bnum);
	printf("Block length            : %d Bytes\n", (int)bsiz);
	printf("Unit Capacity           : %d MBytes %d Bytes\n",
		(int)(bsiz * bnum / 1024 / 1024),
		(int)(bsiz * bnum));

	// Get the restore file size
	if (restore) {
		size = fio.GetFileSize();
		printf("Restore file size       : %d bytes", (int)size);
		if (size > (off_t)(bsiz * bnum)) {
			printf("(WARNING : File size is larger than disk size)");
		} else if (size < (off_t)(bsiz * bnum)) {
			printf("(ERROR   : File size is smaller than disk size)\n");
			goto cleanup_exit;
		}
		printf("\n");
	}

	// Dump by buffer size
	duni = BUFSIZE;
	duni /= bsiz;
	dsiz = BUFSIZE;
	dnum = bnum * bsiz;
	dnum /= BUFSIZE;

	if (restore) {
		printf("Restore progress        : ");
	} else {
		printf("Dump progress           : ");
	}

	for (i = 0; i < (int)dnum; i++) {
		if (i > 0) {
			printf("\033[21D");
			printf("\033[0K");
		}
		printf("%3d%%(%7d/%7d)",
			(int)((i + 1) * 100 / dnum),
			(int)(i * duni),
			(int)bnum);
		fflush(stdout);

		if (restore) {
			if (fio.Read(buffer.data(), dsiz) && Write10(targetid, i * duni, duni, dsiz, buffer.data()) >= 0) {
				continue;
			}
		} else {
			if (Read10(targetid, i * duni, duni, dsiz, buffer.data()) >= 0 && fio.Write(buffer.data(), dsiz)) {
				continue;
			}
		}

		printf("\n");
		printf("Error occured and aborted... %d\n", result);
		goto cleanup_exit;
	}

	if (dnum > 0) {
		printf("\033[21D");
		printf("\033[0K");
	}

	// Rounding on capacity
	dnum = bnum % duni;
	dsiz = dnum * bsiz;
	if (dnum > 0) {
		if (restore) {
			if (fio.Read(buffer.data(), dsiz)) {
				Write10(targetid, i * duni, dnum, dsiz, buffer.data());
			}
		} else {
			if (Read10(targetid, i * duni, dnum, dsiz, buffer.data()) >= 0) {
				fio.Write(buffer.data(), dsiz);
			}
		}
	}

	// Completion Message
	printf("%3d%%(%7d/%7d)\n", 100, (int)bnum, (int)bnum);

cleanup_exit:
	// File close
	fio.Close();

	// Cleanup
	Cleanup();

	// end
	exit(0);
}
