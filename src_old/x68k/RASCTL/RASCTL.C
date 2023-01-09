//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//	Powered by XM6 TypeG Technology.
//
//	Copyright (C) 2016-2021 GIMONS(Twitter:@kugimoto0715)
//
//	[ Sending commands ]
//
//---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/dos.h>
#include <sys/scsi.h>
#include <iocslib.h>

//---------------------------------------------------------------------------
//
//	Primitive definitions
//
//---------------------------------------------------------------------------
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned long long UL64;
typedef int BOOL;
typedef char TCHAR;
typedef char *LPTSTR;
typedef const char *LPCTSTR;
typedef const char *LPCSTR;

#if !defined(FALSE)
#define FALSE		0
#endif

#if !defined(TRUE)
#define TRUE		1
#endif

//---------------------------------------------------------------------------
//
//	Constant definitions
//
//---------------------------------------------------------------------------
#define ENOTCONN	107

//---------------------------------------------------------------------------
//
//	Struct definitions
//
//---------------------------------------------------------------------------
typedef struct
{
	char DeviceType;
	char RMB;
	char ANSI_Ver;
	char RDF;
	char AddLen;
	char RESV0;
	char RESV1;
	char OptFunc;
	char VendorID[8];
	char ProductID[16];
	char FirmRev[4];
} INQUIRY_T;

typedef struct
{
	INQUIRY_T info;
	char options[8];
} INQUIRYOPT_T;

//---------------------------------------------------------------------------
//
//	Global variables
//
//---------------------------------------------------------------------------
int scsiid = -1;

//---------------------------------------------------------------------------
//
//	Search for bridge device
//
//---------------------------------------------------------------------------
BOOL SearchRaSCSI()
{
	int i;
	INQUIRYOPT_T inq;

	for (i = 7; i >= 0; i--) {
		// Search for bridge device
		if (S_INQUIRY(sizeof(INQUIRY_T) , i, (struct INQUIRY*)&inq) < 0) {
			continue;
		}

		if (memcmp(&(inq.info.ProductID), "RASCSI BRIDGE", 13) != 0) {
			continue;
		}

		// Check if option functions are initialized
		if (S_INQUIRY(sizeof(INQUIRYOPT_T) , i, (struct INQUIRY*)&inq) < 0) {
			continue;
		}
		
		if (inq.options[0] != '1') {
			continue;
		}

		// Confirm SCSI ID
		scsiid = i;
		return TRUE;
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Get messages
//
//---------------------------------------------------------------------------
BOOL ScsiGetMessage(BYTE *buf)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return FALSE;
	}

	cmd[0] = 0x28;
	cmd[1] = 0;
	cmd[2] = 0;
	cmd[3] = 0;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0x04;
	cmd[8] = 0;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return FALSE;
	}

	if (S_DATAIN_P(1024, buf) != 0) {
		return FALSE;
	}

	S_STSIN(&sts);
	S_MSGIN(&msg);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Send messages
//
//---------------------------------------------------------------------------
BOOL ScsiSendMessage(BYTE *buf)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return FALSE;
	}

	cmd[0] = 0x2a;
	cmd[1] = 0;
	cmd[2] = 0;
	cmd[3] = 0;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0x04;
	cmd[8] = 0;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return FALSE;
	}

	S_DATAOUT_P(1024, buf);

	S_STSIN(&sts);
	S_MSGIN(&msg);
	
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Send commands
//
//---------------------------------------------------------------------------
BOOL SendCommand(char *buf)
{
	BYTE message[1024];
	
	memset(message, 0x00, 1024);
	memcpy(message, buf, strlen(buf));
	
	if (ScsiSendMessage(message)) {
		memset(message, 0x00, 1024);
		if (ScsiGetMessage(message)) {
			printf("%s", (char*)message);
			return TRUE;
		}
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Main process
//
//---------------------------------------------------------------------------
void main(int argc, char* argv[])
{
	int opt;
	int id;
	int un;
	int cmd;
	int type;
	char *file;
	int len;
	char *ext;
	char buf[BUFSIZ];

	id = -1;
	un = 0;
	cmd = -1;
	type = -1;
	file = NULL;

	// Display help
	if (argc < 2) {
		fprintf(stderr, "SCSI Target Emulator RaSCSI Controller\n");
		fprintf(stderr,
			"Usage: %s -i ID [-u UNIT] [-c CMD] [-t TYPE] [-f FILE]\n",
			argv[0]);
		fprintf(stderr, " where  ID := {0|1|2|3|4|5|6|7}\n");
		fprintf(stderr, "        UNIT := {0|1} default setting is 0.\n");
		fprintf(stderr, "        CMD := {attach|detach|insert|eject|protect}\n");
		fprintf(stderr, "        TYPE := {hd|mo|cd|bridge}\n");
		fprintf(stderr, "        FILE := image file path\n");
		fprintf(stderr, " CMD is 'attach' or 'insert' and FILE parameter is required.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Usage: %s -l\n\n", argv[0]);
		fprintf(stderr, "       Print device list.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Usage: %s --stop\n\n", argv[0]);
		fprintf(stderr, "       Stop rascsi prosess.\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Usage: %s --shutdown\n\n", argv[0]);
		fprintf(stderr, "       Shutdown raspberry pi.\n");
		exit(0);
	}

	// Look for bridge device
	if (!SearchRaSCSI()) {
		fprintf(stderr, "Error: bridge not found\n");
		exit(ENOTCONN);
	}

	// Argument parsing
	opterr = 0;
	while ((opt = getopt(argc, argv, "i:u:c:t:f:l-:")) != -1) {
		switch (opt) {
			case 'i':
				id = optarg[0] - '0';
				break;

			case 'u':
				un = optarg[0] - '0';
				break;

			case 'c':
				switch (optarg[0]) {
					case 'a':				// ATTACH
					case 'A':
						cmd = 0;
						break;
					case 'd':				// DETACH
					case 'D':
						cmd = 1;
						break;
					case 'i':				// INSERT
					case 'I':
						cmd = 2;
						break;
					case 'e':				// EJECT
					case 'E':
						cmd = 3;
						break;
					case 'p':				// PROTECT
					case 'P':
						cmd = 4;
						break;
				}
				break;

			case 't':
				switch (optarg[0]) {
					case 's':				// HD(SASI)
					case 'S':
					case 'h':				// HD(SCSI)
					case 'H':
						type = 0;
						break;
					case 'm':				// MO
					case 'M':
						type = 2;
						break;
					case 'c':				// CD
					case 'C':
						type = 3;
						break;
					case 'b':				// BRIDGE
					case 'B':
						type = 4;
						break;
				}
				break;

			case 'f':
				file = optarg;
				break;

			case 'l':
				sprintf(buf, "list\n");
				SendCommand(buf);
				exit(0);

			case '-':
				if (strcmp(optarg, "shutdown") == 0) {
					sprintf(buf, "shutdown\n");
					SendCommand(buf);
					exit(0);
				} else if (strcmp(optarg, "stop") == 0) {
					sprintf(buf, "stop\n");
					SendCommand(buf);
					exit(0);
				}
				break;
		}
	}

	// ID check
	if (id < 0 || id > 7) {
		fprintf(stderr, "Error : Invalid ID\n");
		exit(EINVAL);
	}

	// Unit check
	if (un < 0 || un > 1) {
		fprintf(stderr, "Error : Invalid UNIT\n");
		exit(EINVAL);
	}

	// Command check
	if (cmd < 0) {
		cmd = 0;	// The default is 'attach'
	}

	// Type check
	if (cmd == 0 && type < 0) {

		// Attempt type detection from extension
		len = file ? strlen(file) : 0;
		if (len > 4 && file[len - 4] == '.') {
			ext = &file[len - 3];
			if (strcasecmp(ext, "hdf") == 0 ||
				strcasecmp(ext, "hds") == 0 ||
				strcasecmp(ext, "hdn") == 0 ||
				strcasecmp(ext, "hdi") == 0 ||
				strcasecmp(ext, "nhd") == 0 ||
				strcasecmp(ext, "hda") == 0) {
				// HD(SASI/SCSI)
				type = 0;
			} else if (strcasecmp(ext, "mos") == 0) {
				// MO
				type = 2;
			} else if (strcasecmp(ext, "iso") == 0) {
				// CD
				type = 3;
			}
		}

		if (type < 0) {
			fprintf(stderr, "Error : Invalid type\n");
			exit(EINVAL);
		}
	}

	// File check (the command is 'attach' with type HD)
	if (cmd == 0 && type >= 0 && type <= 1) {
		if (!file) {
			fprintf(stderr, "Error : Invalid file path\n");
			exit(EINVAL);
		}
	}

	// File check (the command is 'insert')
	if (cmd == 2) {
		if (!file) {
			fprintf(stderr, "Error : Invalid file path\n");
			exit(EINVAL);
		}
	}

	// Useless types are set to 0
	if (type < 0) {
		type = 0;
	}

	// Create command to send
	sprintf(buf, "%d %d %d %d %s\n", id, un, cmd, type, file ? file : "-");
	if (!SendCommand(buf)) {
		exit(ENOTCONN);
	}

	// Quit
	exit(0);
}
