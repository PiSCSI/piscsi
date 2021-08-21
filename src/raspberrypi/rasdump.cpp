//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ HDD dump utility (initiator mode) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "fileio.h"
#include "filepath.h"
#include "gpiobus.h"
#include "rascsi_version.h"

//---------------------------------------------------------------------------
//
//	Constant Declaration
//
//---------------------------------------------------------------------------
#define BUFSIZE 1024 * 64			// Buffer size of about 64KB

//---------------------------------------------------------------------------
//
//	Variable Declaration
//
//---------------------------------------------------------------------------
GPIOBUS bus;						// Bus
int targetid;						// Target ID
int boardid;						// Board ID (own ID)
Filepath hdsfile;					// HDS file
bool restore;						// Restore flag
BYTE buffer[BUFSIZE];					// Work Buffer
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
void KillHandler(int sig)
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
bool Banner(int argc, char* argv[])
{
	printf("RaSCSI hard disk dump utility ");
	printf("version %s (%s, %s)\n",
		rascsi_get_version_string(),
		__DATE__,
		__TIME__);

	if (argc < 2 || strcmp(argv[1], "-h") == 0) {
		printf("Usage: %s -i ID [-b BID] -f FILE [-r]\n", argv[0]);
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
bool Init()
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

	// GPIO Initialization
	if (!bus.Init(BUS::INITIATOR)) {
		return false;
	}

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
	bus.Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void Reset()
{
	// Reset the bus signal line
	bus.Reset();
}

//---------------------------------------------------------------------------
//
//	Argument processing
//
//---------------------------------------------------------------------------
bool ParseArgument(int argc, char* argv[])
{
	int opt;
	char *file;

	// Initialization
	file = NULL;

	// Argument Parsing
	opterr = 0;
	while ((opt = getopt(argc, argv, "i:b:f:r")) != -1) {
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

	hdsfile.SetPath(file);

	return true;
}

//---------------------------------------------------------------------------
//
//	Wait Phase
//
//---------------------------------------------------------------------------
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
//	Bus Free Phase
//
//---------------------------------------------------------------------------
void BusFree()
{
	// Bus Reset
	bus.Reset();
}

//---------------------------------------------------------------------------
//
//	Selection Phase
//
//---------------------------------------------------------------------------
bool Selection(int id)
{
	BYTE data;
	int count;

	// ID setting and SEL assert
	data = 0;
	data |= (1 << boardid);
	data |= (1 << id);
	bus.SetDAT(data);
	bus.SetSEL(TRUE);

	// wait for busy
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

	// Success if the target is busy
	return bus.GetBSY();
}

//---------------------------------------------------------------------------
//
//	Command Phase
//
//---------------------------------------------------------------------------
bool Command(BYTE *buf, int length)
{
	int count;

	// Waiting for Phase
	if (!WaitPhase(BUS::command)) {
		return false;
	}

	// Send Command
	count = bus.SendHandShake(buf, length);

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
int DataIn(BYTE *buf, int length)
{
	// Wait for phase
	if (!WaitPhase(BUS::datain)) {
		return -1;
	}

	// Data reception
	return bus.ReceiveHandShake(buf, length);
}

//---------------------------------------------------------------------------
//
//	Data out phase
//
//---------------------------------------------------------------------------
int DataOut(BYTE *buf, int length)
{
	// Wait for phase
	if (!WaitPhase(BUS::dataout)) {
		return -1;
	}

	// Data transmission
	return bus.SendHandShake(buf, length);
}

//---------------------------------------------------------------------------
//
//	Status Phase
//
//---------------------------------------------------------------------------
int Status()
{
	BYTE buf[256];

	// Wait for phase
	if (!WaitPhase(BUS::status)) {
		return -2;
	}

	// Data reception
	if (bus.ReceiveHandShake(buf, 1) == 1) {
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
int MessageIn()
{
	BYTE buf[256];

	// Wait for phase
	if (!WaitPhase(BUS::msgin)) {
		return -2;
	}

	// Data reception
	if (bus.ReceiveHandShake(buf, 1) == 1) {
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
int TestUnitReady(int id)
{
	BYTE cmd[256];

	// Result code initialization
	result = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 6);
	cmd[0] = 0x00;
	if (!Command(cmd, 6)) {
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
int RequestSense(int id, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// Result code initialization
	result = 0;
	count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 6);
	cmd[0] = 0x03;
	cmd[4] = 0xff;
	if (!Command(cmd, 6)) {
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
int ModeSense(int id, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// Result code initialization
	result = 0;
	count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 6);
	cmd[0] = 0x1a;
	cmd[2] = 0x3f;
	cmd[4] = 0xff;
	if (!Command(cmd, 6)) {
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
int Inquiry(int id, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// Result code initialization
	result = 0;
	count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 6);
	cmd[0] = 0x12;
	cmd[4] = 0xff;
	if (!Command(cmd, 6)) {
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
int ReadCapacity(int id, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// Result code initialization
	result = 0;
	count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 10);
	cmd[0] = 0x25;
	if (!Command(cmd, 10)) {
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
int Read10(int id, DWORD bstart, DWORD blength, DWORD length, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// Result code initialization
	result = 0;
	count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 10);
	cmd[0] = 0x28;
	cmd[2] = (BYTE)(bstart >> 24);
	cmd[3] = (BYTE)(bstart >> 16);
	cmd[4] = (BYTE)(bstart >> 8);
	cmd[5] = (BYTE)bstart;
	cmd[7] = (BYTE)(blength >> 8);
	cmd[8] = (BYTE)blength;
	if (!Command(cmd, 10)) {
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
int Write10(int id, DWORD bstart, DWORD blength, DWORD length, BYTE *buf)
{
	BYTE cmd[256];
	int count;

	// Result code initialization
	result = 0;
	count = 0;

	// SELECTION
	if (!Selection(id)) {
		result = -1;
		goto exit;
	}

	// COMMAND
	memset(cmd, 0x00, 10);
	cmd[0] = 0x2a;
	cmd[2] = (BYTE)(bstart >> 24);
	cmd[3] = (BYTE)(bstart >> 16);
	cmd[4] = (BYTE)(bstart >> 8);
	cmd[5] = (BYTE)bstart;
	cmd[7] = (BYTE)(blength >> 8);
	cmd[8] = (BYTE)blength;
	if (!Command(cmd, 10)) {
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
int main(int argc, char* argv[])
{
	int i;
	int count;
	char str[32];
	DWORD bsiz;
	DWORD bnum;
	DWORD duni;
	DWORD dsiz;
	DWORD dnum;
	Fileio fio;
	Fileio::OpenMode omode;
	off_t size;

	// Banner output
	if (!Banner(argc, argv)) {
		exit(0);
	}

	// Initialization
	if (!Init()) {
		fprintf(stderr, "Error : Initializing. Are you root?\n");

		// Probably not root
		exit(EPERM);
	}

	// Prase Argument
	if (!ParseArgument(argc, argv)) {
		// Cleanup
		Cleanup();

		// Exit with invalid argument error
		exit(EINVAL);
	}

	// Reset the SCSI bus
	Reset();

	// File Open
	if (restore) {
		omode = Fileio::ReadOnly;
	} else {
		omode = Fileio::WriteOnly;
	}
	if (!fio.Open(hdsfile.GetPath(), omode)) {
		fprintf(stderr, "Error : Can't open hds file\n");

		// Cleanup
		Cleanup();
		exit(EPERM);
	}

	// Bus free
	BusFree();

	// Assert reset signal
	bus.SetRST(TRUE);
	usleep(1000);
	bus.SetRST(FALSE);

	// Start dump
	printf("TARGET ID               : %d\n", targetid);
	printf("BORAD ID                : %d\n", boardid);

	// TEST UNIT READY
	count = TestUnitReady(targetid);
	if (count < 0) {
		fprintf(stderr, "TEST UNIT READY ERROR %d\n", count);
		goto cleanup_exit;
	}

	// REQUEST SENSE(for CHECK CONDITION)
	count = RequestSense(targetid, buffer);
	if (count < 0) {
		fprintf(stderr, "REQUEST SENSE ERROR %d\n", count);
		goto cleanup_exit;
	}

	// INQUIRY
	count = Inquiry(targetid, buffer);
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
	count = ReadCapacity(targetid, buffer);
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
			if (fio.Read(buffer, dsiz)) {
				if (Write10(targetid, i * duni, duni, dsiz, buffer) >= 0) {
					continue;
				}
			}
		} else {
			if (Read10(targetid, i * duni, duni, dsiz, buffer) >= 0) {
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

	// Rounding on capacity
	dnum = bnum % duni;
	dsiz = dnum * bsiz;
	if (dnum > 0) {
		if (restore) {
			if (fio.Read(buffer, dsiz)) {
				Write10(targetid, i * duni, dnum, dsiz, buffer);
			}
		} else {
			if (Read10(targetid, i * duni, dnum, dsiz, buffer) >= 0) {
				fio.Write(buffer, dsiz);
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
