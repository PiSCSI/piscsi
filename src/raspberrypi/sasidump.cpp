//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ HDD dump utility (Initiator mode/SASI Version) ]
//
//	SASI IMAGE EXAMPLE
//		X68000
//			10MB(10441728 BS=256 C=40788)
//			20MB(20748288 BS=256 C=81048)
//			40MB(41496576 BS=256 C=162096)
//
//		MZ-2500/MZ-2800 MZ-1F23
//			20MB(22437888 BS=1024 C=21912)
//
//---------------------------------------------------------------------------

#include <cerrno>
#include "os.h"
#include "fileio.h"
#include "filepath.h"
#include "gpiobus.h"
#include "rascsi.h"
#include "rascsi_version.h"

#define BUFSIZE 1024 * 64			// Maybe around 64KB?

GPIOBUS bus;
int targetid;
int unitid;
int bsiz;							// Block size
int bnum;							// Number of blocks
Filepath hdffile;					// HDF file
bool restore;						// Restore flag
BYTE buffer[BUFSIZE];				// Work buffer
int result;							// Result code

void Cleanup();

//---------------------------------------------------------------------------
//
// Signal processing
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
	// Stop instruction
	Cleanup();
	exit(0);
}

//---------------------------------------------------------------------------
//
// Banner output
//
//---------------------------------------------------------------------------
bool Banner(int argc, char* argv[])
{
	printf("RaSCSI hard disk dump utility (SASI HDD) ");
	printf("version %s (%s, %s)\n",
			rascsi_get_version_string(),
			__DATE__,
			__TIME__);

	if (argc < 2 || strcmp(argv[1], "-h") == 0) {
		printf("Usage: %s -i ID [-u UT] [-b BSIZE] -c COUNT -f FILE [-r]\n", argv[0]);
		printf(" ID is target device SASI ID {0|1|2|3|4|5|6|7}.\n");
		printf(" UT is target unit ID {0|1}. Default is 0.\n");
		printf(" BSIZE is block size {256|512|1024}. Default is 256.\n");
		printf(" COUNT is block count.\n");
		printf(" FILE is HDF file path.\n");
		printf(" -r is restore operation.\n");
		return false;
	}

	return true;
}

bool Init()
{
	// Interrupt handler settings
	if (signal(SIGINT, KillHandler) == SIG_ERR) {
		return false;
	}
	if (signal(SIGHUP, KillHandler) == SIG_ERR) {
		return false;
	}
	if (signal(SIGTERM, KillHandler) == SIG_ERR) {
		return false;
	}

	// GPIO initialization
	if (!bus.Init(BUS::INITIATOR)) {
		return false;
	}

	// Work initialization
	targetid = -1;
	unitid = 0;
	bsiz = 256;
	bnum = -1;
	restore = false;

	return true;
}

void Cleanup()
{
	bus.Cleanup();
}

void Reset()
{
	// Reset bus signal line
	bus.Reset();
}

bool ParseArgument(int argc, char* argv[])
{
	int opt;
	char *file;

	// Initialization
	file = NULL;

	// Argument parsing
	opterr = 0;
	while ((opt = getopt(argc, argv, "i:u:b:c:f:r")) != -1) {
		switch (opt) {
			case 'i':
				targetid = optarg[0] - '0';
				break;

			case 'u':
				unitid = optarg[0] - '0';
				break;

			case 'b':
				bsiz = atoi(optarg);
				break;

			case 'c':
				bnum = atoi(optarg);
				break;

			case 'f':
				file = optarg;
				break;

			case 'r':
				restore = true;
				break;
		}
	}

	// TARGET ID check
	if (targetid < 0 || targetid > 7) {
		fprintf(stderr,
			"Error : Invalid target id range\n");
		return false;
	}

	// UNIT ID check
	if (unitid < 0 || unitid > 1) {
		fprintf(stderr,
			"Error : Invalid unit id range\n");
		return false;
	}

	// BSIZ check
	if (bsiz != 256 && bsiz != 512 && bsiz != 1024) {
		fprintf(stderr,
			"Error : Invalid block size\n");
		return false;
	}

	// BNUM check
	if (bnum < 0) {
		fprintf(stderr,
			"Error : Invalid block count\n");
		return false;
	}

	// File check
	if (!file) {
		fprintf(stderr,
			"Error : Invalid file path\n");
		return false;
	}

	hdffile.SetPath(file);

	return true;
}

bool WaitPhase(BUS::phase_t phase)
{
	DWORD now;

	// Timeout (3000ms)
	now = SysTimer::GetTimerLow();
	while ((SysTimer::GetTimerLow() - now) < 3 * 1000 * 1000) {
		bus.Aquire();
		if (bus.GetREQ() && bus.GetPhase() == phase) {
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//
//	Bus free phase execution
//
//---------------------------------------------------------------------------
void BusFree()
{
	bus.Reset();
}

//---------------------------------------------------------------------------
//
// Selection phase execution
//
//---------------------------------------------------------------------------
bool Selection(int id)
{
	BYTE data;
	int count;

	// ID setting and SEL assertion
	data = 0;
	data |= (1 << id);
	bus.SetDAT(data);
	bus.SetSEL(TRUE);

	// Wait for BSY
	count = 10000;
	do {
		usleep(20);
		bus.Aquire();
		if (bus.GetBSY()) {
			break;
		}
	} while (count--);

	// SEL negate
	bus.SetSEL(FALSE);

	// Return true if target is busy
	return bus.GetBSY();
}

//---------------------------------------------------------------------------
//
// Command phase execution
//
//---------------------------------------------------------------------------
bool Command(BYTE *buf, int length)
{
	int count;

	// Wait for phase
	if (!WaitPhase(BUS::command)) {
		return false;
	}

	// Send command
	count = bus.SendHandShake(buf, length, BUS::SEND_NO_DELAY);

	// Return true is send results match number of requests
	if (count == length) {
		return true;
	}

	// Send error
	return false;
}

//---------------------------------------------------------------------------
//
// Data in phase execution
//
//---------------------------------------------------------------------------
int DataIn(BYTE *buf, int length)
{
	// Wait for phase
	if (!WaitPhase(BUS::datain)) {
		return -1;
	}

	// Receive data
	return bus.ReceiveHandShake(buf, length);
}

//---------------------------------------------------------------------------
//
// Data out phase execution
//
//---------------------------------------------------------------------------
int DataOut(BYTE *buf, int length)
{
	// Wait for phase
	if (!WaitPhase(BUS::dataout)) {
		return -1;
	}

	// Receive data
	return bus.SendHandShake(buf, length, BUS::SEND_NO_DELAY);
}

//---------------------------------------------------------------------------
//
//	Status phase execution
//
//---------------------------------------------------------------------------
int Status()
{
	BYTE buf[256];

	// Wait for phase
	if (!WaitPhase(BUS::status)) {
		return -2;
	}

	// Receive data
	if (bus.ReceiveHandShake(buf, 1) == 1) {
		return (int)buf[0];
	}

	// Receive error
	return -1;
}

//---------------------------------------------------------------------------
//
//	Message in phase execution
//
//---------------------------------------------------------------------------
int MessageIn()
{
	BYTE buf[256];

	// Wait for phase
	if (!WaitPhase(BUS::msgin)) {
		return -2;
	}

	// Receive data
	if (bus.ReceiveHandShake(buf, 1) == 1) {
		return (int)buf[0];
	}

	// Receive error
	return -1;
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY execution
//
//---------------------------------------------------------------------------
int TestUnitReady(int id)
{
	BYTE cmd[256];

	// Initialize result code
	result = 0;

	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	memset(cmd, 0x00, 6);
	cmd[0] = 0x00;
	cmd[1] = unitid << 5;
	if (!Command(cmd, 6)) {
		result = -2;
		goto exit;
	}

	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	BusFree();

	return result;
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE execution
//
//---------------------------------------------------------------------------
int RequestSense(int id, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// Initialize result codes
	result = 0;
	count = 0;

	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	memset(cmd, 0x00, 6);
	cmd[0] = 0x03;
	cmd[1] = unitid << 5;
	cmd[4] = 4;
	if (!Command(cmd, 6)) {
		result = -2;
		goto exit;
	}

	memset(buf, 0x00, 256);
	count = DataIn(buf, 256);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	BusFree();

	// If successful, return number of transfers
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	READ6 execution
//
//---------------------------------------------------------------------------
int Read6(int id, DWORD bstart, DWORD blength, DWORD length, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// Initialize result codes
	result = 0;
	count = 0;

	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	memset(cmd, 0x00, 10);
	cmd[0] = 0x8;
	cmd[1] = (BYTE)(bstart >> 16);
	cmd[1] &= 0x1f;
	cmd[1] = unitid << 5;
	cmd[2] = (BYTE)(bstart >> 8);
	cmd[3] = (BYTE)bstart;
	cmd[4] = (BYTE)blength;
	if (!Command(cmd, 6)) {
		result = -2;
		goto exit;
	}

	count = DataIn(buf, length);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	BusFree();

	// If successful, return number of transfers
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	WRITE6 execution
//
//---------------------------------------------------------------------------
int Write6(int id, DWORD bstart, DWORD blength, DWORD length, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// Initialize result codes
	result = 0;
	count = 0;

	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	memset(cmd, 0x00, 10);
	cmd[0] = 0xa;
	cmd[1] = (BYTE)(bstart >> 16);
	cmd[1] &= 0x1f;
	cmd[1] = unitid << 5;
	cmd[2] = (BYTE)(bstart >> 8);
	cmd[3] = (BYTE)bstart;
	cmd[4] = (BYTE)blength;
	if (!Command(cmd, 6)) {
		result = -2;
		goto exit;
	}

	count = DataOut(buf, length);
	if (count <= 0) {
		result = -3;
		goto exit;
	}

	if (Status() < 0) {
		result = -4;
		goto exit;
	}

	if (MessageIn() < 0) {
		result = -5;
		goto exit;
	}

exit:
	BusFree();

	// If successful, return number of transfers
	if (result == 0) {
		return count;
	}

	return result;
}

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	int i;
	int count;
	DWORD duni;
	DWORD dsiz;
	DWORD dnum;
	Fileio fio;
	Fileio::OpenMode omode;
	off_t size;

	// Output banner
	if (!Banner(argc, argv)) {
		exit(0);
	}

	if (!Init()) {
		fprintf(stderr, "Error : Initializing\n");

		// Probably not executing as root?
		exit(EPERM);
	}

	if (!ParseArgument(argc, argv)) {
		Cleanup();

		// Exit with argument error
		exit(EINVAL);
	}

	Reset();

	// Open file
	if (restore) {
		omode = Fileio::ReadOnly;
	} else {
		omode = Fileio::WriteOnly;
	}
	if (!fio.Open(hdffile.GetPath(), omode)) {
		fprintf(stderr, "Error : Can't open hdf file\n");

		Cleanup();
		exit(EPERM);
	}

	BusFree();

	// Execute RESET signal
	bus.SetRST(TRUE);
	usleep(1000);
	bus.SetRST(FALSE);

	// Start dump
	printf("TARGET ID               : %d\n", targetid);
	printf("UNIT ID                 : %d\n", unitid);

	// TEST UNIT READY
	count = TestUnitReady(targetid);
	if (count < 0) {
		fprintf(stderr, "TEST UNIT READY ERROR %d\n", count);
		goto cleanup_exit;
	}

	// REQUEST SENSE (for CHECK CONDITION)
	count = RequestSense(targetid, buffer);
	if (count < 0) {
		fprintf(stderr, "REQUEST SENSE ERROR %d\n", count);
		goto cleanup_exit;
	}

	printf("Number of blocks        : %d Blocks\n", bnum);
	printf("Block length            : %d Bytes\n", bsiz);

	// Display data size
	printf("Total length            : %d MBytes %d Bytes\n",
		(bsiz * bnum / 1024 / 1024),
		(bsiz * bnum));

	// Get restore file size
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
			bnum);
		fflush(stdout);

		if (restore) {
			if (fio.Read(buffer, dsiz)) {
				if (Write6(targetid, i * duni, duni, dsiz, buffer) >= 0) {
					continue;
				}
			}
		} else {
			if (Read6(targetid, i * duni, duni, dsiz, buffer) >= 0) {
				if (fio.Write(buffer, dsiz)) {
					continue;
				}
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

	// Rounding of capacity
	dnum = bnum % duni;
	dsiz = dnum * bsiz;

	if (dnum > 0) {
		if (restore) {
			if (fio.Read(buffer, dsiz)) {
				Write6(targetid, i * duni, dnum, dsiz, buffer);
			}
		} else {
			if (Read6(targetid, i * duni, dnum, dsiz, buffer) >= 0) {
				fio.Write(buffer, dsiz);
			}
		}
	}

	// Completion message
	printf("%3d%%(%7d/%7d)\n", 100, bnum, bnum);

cleanup_exit:
	fio.Close();

	Cleanup();

	exit(0);
}
