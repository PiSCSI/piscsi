//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ Send Control Command ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "rascsi_version.h"

//---------------------------------------------------------------------------
//
//	Send Command
//
//---------------------------------------------------------------------------
BOOL SendCommand(char *buf)
{
	int fd;
	struct sockaddr_in server;
	FILE *fp;

	// Create a socket to send the command
	fd = socket(PF_INET, SOCK_STREAM, 0);
	memset(&server, 0, sizeof(server));
	server.sin_family = PF_INET;
	server.sin_port   = htons(6868);
	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 

	// Connect
	if (connect(fd, (struct sockaddr *)&server,
		sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr, "Error : Can't connect to rascsi process\n");
		return FALSE;
	}

	// Send the command
	fp = fdopen(fd, "r+");
	setvbuf(fp, NULL, _IONBF, 0);
	fputs(buf, fp);

	// Receive the message
	while (1) {
		if (fgets((char *)buf, BUFSIZ, fp) == NULL) {
			break;
		}
		printf("%s", buf);
	}

	// Close the socket when we're done
	fclose(fp);
	close(fd);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	int opt;
	int id;
	int un;
	int cmd;
	int type;
	char *file;
	BOOL list;
	int len;
	char *ext;
	char buf[BUFSIZ];

	id = -1;
	un = 0;
	cmd = -1;
	type = -1;
	file = NULL;
	list = FALSE;

	// Display help
	if (argc < 2) {
		fprintf(stderr, "SCSI Target Emulator RaSCSI Controller\n");
		fprintf(stderr, "version %s (%s, %s)\n",
			rascsi_get_version_string(),
			__DATE__,
			__TIME__);
		fprintf(stderr,
			"Usage: %s -i ID [-u UNIT] [-c CMD] [-t TYPE] [-f FILE]\n",
			argv[0]);
		fprintf(stderr, " where  ID := {0|1|2|3|4|5|6|7}\n");
		fprintf(stderr, "        UNIT := {0|1} default setting is 0.\n");
		fprintf(stderr, "        CMD := {attach|detach|insert|eject|protect}\n");
		fprintf(stderr, "        TYPE := {hd|mo|cd|bridge}\n");
		fprintf(stderr, "        FILE := image file path\n");
		fprintf(stderr, " CMD is 'attach' or 'insert' and FILE parameter is required.\n");
		fprintf(stderr, "Usage: %s -l\n", argv[0]);
		fprintf(stderr, "       Print device list.\n");
		exit(0);
	}

	// Parse the arguments
	opterr = 0;
	while ((opt = getopt(argc, argv, "i:u:c:t:f:l")) != -1) {
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
				list = TRUE;
				break;
		}
	}

	// List display only
	if (id < 0 && cmd < 0 && type < 0 && file == NULL && list) {
		sprintf(buf, "list\n");
		SendCommand(buf);
		exit(0);
	}

	// Check the ID number
	if (id < 0 || id > 7) {
		fprintf(stderr, "Error : Invalid ID\n");
		exit(EINVAL);
	}

	// Check the unit number
	if (un < 0 || un > 1) {
		fprintf(stderr, "Error : Invalid UNIT\n");
		exit(EINVAL);
	}

	// Command check
	if (cmd < 0) {
		cmd = 0;	// Default command is ATTATCH
	}

	// Type Check
	if (cmd == 0 && type < 0) {

		// Try to determine the file type from the extension
		len = file ? strlen(file) : 0;
		if (len > 4 && file[len - 4] == '.') {
			ext = &file[len - 3];
			if (xstrcasecmp(ext, "hdf") == 0 ||
				xstrcasecmp(ext, "hds") == 0 ||
				xstrcasecmp(ext, "hdn") == 0 ||
				xstrcasecmp(ext, "hdi") == 0 ||
				xstrcasecmp(ext, "nhd") == 0 ||
				xstrcasecmp(ext, "hda") == 0) {
				// HD(SASI/SCSI)
				type = 0;
			} else if (xstrcasecmp(ext, "mos") == 0) {
				// MO
				type = 2;
			} else if (xstrcasecmp(ext, "iso") == 0) {
				// CD
				type = 3;
			}
		}

		if (type < 0) {
			fprintf(stderr, "Error : Invalid type\n");
			exit(EINVAL);
		}
	}

	// File check (command is ATTACH and type is HD)
	if (cmd == 0 && type >= 0 && type <= 1) {
		if (!file) {
			fprintf(stderr, "Error : Invalid file path\n");
			exit(EINVAL);
		}
	}

	// File check (command is INSERT)
	if (cmd == 2) {
		if (!file) {
			fprintf(stderr, "Error : Invalid file path\n");
			exit(EINVAL);
		}
	}

	// Set unnecessary type to 0
	if (type < 0) {
		type = 0;
	}

	// Generate the command and send it
	sprintf(buf, "%d %d %d %d %s\n", id, un, cmd, type, file ? file : "-");
	if (!SendCommand(buf)) {
		exit(ENOTCONN);
	}

	// Display the list
	if (list) {
		sprintf(buf, "list\n");
		SendCommand(buf);
	}

	// All done!
	exit(0);
}
