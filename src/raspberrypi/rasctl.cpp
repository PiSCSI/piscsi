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
#include "rasctl_interface.pb.h"

using namespace std;
using namespace rasctl_interface;

//---------------------------------------------------------------------------
//
//	Send Command
//
//---------------------------------------------------------------------------
bool SendCommand(const Command& command)
{
	// Create a socket to send the command
	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));
	server.sin_family = PF_INET;
	server.sin_port   = htons(6868);
	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	int fd = socket(PF_INET, SOCK_STREAM, 0);

	if (connect(fd, (struct sockaddr *)&server,
		sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr, "Error : Can't connect to rascsi process\n");
		return false;
	}

	// Send the command
	FILE *fp = fdopen(fd, "r+");
	setvbuf(fp, NULL, _IONBF, 0);

	// Serialize the command in binary format: Length followed by the actual data
	string data;
    command.SerializeToString(&data);
    int len = data.length();
    fwrite(&len, sizeof(len), 1, fp);
    char* buf = (char *)malloc(data.length());
    memcpy(buf, data.data(), data.length());
    fwrite(buf, data.length(), 1, fp);
    free(buf);

    bool status = true;

	// Receive the message
	while (status) {
		int len;
		size_t res = fread(&len, sizeof(int), 1, fp);
		if (res != 1) {
			break;
		}

		buf = (char *)malloc(len);
		res = fread(buf, len, 1, fp);
		if (res != 1) {
			free(buf);
			break;
		}

		string msg(buf, len);
		free(buf);
		Result result;
		result.ParseFromString(msg);

		if (!result.status()) {
			cout << result.msg();

			status = false;
		}
	}

	// Close the socket when we're done
	fclose(fp);
	close(fd);

	return status;
}

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	int opt;

	int id = -1;
	int un = 0;
	Opcode cmd = NONE;
	DeviceType type = UNDEFINED;
	string file;
	bool list = false;

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
		fprintf(stderr, "        TYPE := {hd|mo|cd|bridge|daynaport}\n");
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
					case 'a':
					case 'A':
						cmd = ATTACH;
						break;
					case 'd':
					case 'D':
						cmd = DETACH;
						break;
					case 'i':
					case 'I':
						cmd = INSERT;
						break;
					case 'e':
					case 'E':
						cmd = EJECT;
						break;
					case 'p':
					case 'P':
						cmd = PROTECT;
						break;
				}
				break;

			case 't':
				switch (optarg[0]) {
					case 's':
					case 'S':
						type = SASI_HD;
						break;
					case 'h':
					case 'H':
						type = SCSI_HD;
						break;
					case 'm':
					case 'M':
						type = MO;
						break;
					case 'c':
					case 'C':
						type = CD;
						break;
					case 'b':
					case 'B':
						type = BR;
						break;
					// case 'n':
					// case 'N':
					// 	type = NUVOLINK;
					// 	break;
					case 'd':
					case 'D':
						type = DAYNAPORT;
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

	Command command;

	// List display only
	if (id < 0 && cmd == NONE && type == UNDEFINED && file.empty() && list) {
		command.set_cmd(LIST);
		SendCommand(command);
		exit(0);
	}

	// Check the ID number
	if (id < 0 || id > 7) {
		fprintf(stderr, "%s Error : Invalid ID %d \n", __PRETTY_FUNCTION__, id);
		exit(EINVAL);
	}

	// Check the unit number
	if (un < 0 || un > 1) {
		fprintf(stderr, "%s Error : Invalid UNIT %d \n", __PRETTY_FUNCTION__, un);
		exit(EINVAL);
	}

	// Type Check
	if (cmd == ATTACH && type == UNDEFINED) {
		// Try to determine the file type from the extension
		int len = file.length();
		if (len > 4 && file[len - 4] == '.') {
			string ext = file.substr(len - 3);
			if (ext == "hdf" || ext == "hds" || ext == "hdn" || ext == "hdi" || ext == "nhd" || ext == "hda") {
				type = SASI_HD;
			} else if (ext == "mos") {
				type = MO;
			} else if (ext == "iso") {
				type = CD;
			}
		}
	}

	// File check (command is ATTACH and type is HD, CD or MO)
	if (cmd == ATTACH && (type == SASI_HD || type == SCSI_HD || type == MO || type == CD) && file.empty()) {
		fprintf(stderr, "Error : Invalid file path\n");
		exit(EINVAL);
	}

	// File check (command is INSERT)
	if (cmd == INSERT && file.empty()) {
		fprintf(stderr, "Error : Invalid file path\n");
		exit(EINVAL);
	}

	// Generate the command and send it
	command.set_id(id);
	command.set_un(un);
	command.set_cmd(cmd);
	command.set_type(type);
	if (!file.empty()) {
		command.set_file(file);
	}
	if (!SendCommand(command)) {
		exit(ENOTCONN);
	}

	// Display the list
	if (list) {
		command.set_cmd(LIST);
		SendCommand(command);
	}

	// All done!
	exit(0);
}
