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
#include "exceptions.h"
#include "rasutil.h"
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

	if (connect(fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
		cerr << "Error : Can't connect to rascsi process" << endl;
		return false;
	}

	// Send the command
	FILE *fp = fdopen(fd, "r+");
	setvbuf(fp, NULL, _IONBF, 0);

	string data;
    command.SerializeToString(&data);
    SerializeProtobufData(fp, data);

	// Receive the message
    bool status = true;
    try {
    	while (status) {
    		Result result;
    		result.ParseFromString(DeserializeProtobufData(fd));

    		if (!result.status()) {
    			cout << result.msg();

    			status = false;
    		}
    	}
    }
    catch(const ioexception& e) {
    	// Fall through
    }

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
	// Display help
	if (argc < 2) {
		cerr << "SCSI Target Emulator RaSCSI Controller" << endl;
		cerr << "version " << rascsi_get_version_string() << " (" << __DATE__ << ", " << __TIME__ << ")" << endl;
		cerr << "Usage: " << argv[0] << " -i ID [-u UNIT] [-c CMD] [-t TYPE] [-f FILE]" << endl;
		cerr << " where  ID := {0|1|2|3|4|5|6|7}" << endl;
		cerr << "        UNIT := {0|1} default setting is 0." << endl;
		cerr << "        CMD := {attach|detach|insert|eject|protect}" << endl;
		cerr << "        TYPE := {hd|mo|cd|bridge|daynaport}" << endl;
		cerr << "        FILE := image file path" << endl;
		cerr << " CMD is 'attach' or 'insert' and FILE parameter is required." << endl;
		cerr << "Usage: " << argv[0] << " -l" << endl;
		cerr << "       Print device list." << endl;

		exit(0);
	}

	// Parse the arguments
	int opt;
	int id = -1;
	int un = 0;
	Opcode cmd = LIST;
	DeviceType type = UNDEFINED;
	string file;
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
				switch (tolower(optarg[0])) {
					case 'a':
						cmd = ATTACH;
						break;

					case 'd':
						cmd = DETACH;
						break;

					case 'i':
						cmd = INSERT;
						break;

					case 'e':
						cmd = EJECT;
						break;

					case 'p':
						cmd = PROTECT;
						break;
				}
				break;

			case 't':
				switch (tolower(optarg[0])) {
					case 's':
						type = SASI_HD;
						break;

					case 'h':
						type = SCSI_HD;
						break;

					case 'm':
						type = MO;
						break;

					case 'c':
						type = CD;
						break;

					case 'b':
						type = BR;
						break;

					// case 'n':
					// 	type = NUVOLINK;
					// 	break;

					case 'd':
						type = DAYNAPORT;
						break;
				}
				break;

			case 'f':
				file = optarg;
				break;

			case 'l':
				cmd = LIST;
				break;
		}
	}

	Command command;

	// List display only
	if (cmd == LIST || (id < 0 && type == UNDEFINED && file.empty())) {
		command.set_cmd(LIST);
		SendCommand(command);
		exit(0);
	}

	// Check the ID number
	if (id < 0 || id > 7) {
		cerr << __PRETTY_FUNCTION__ << " Error : Invalid ID " << id << endl;
		exit(EINVAL);
	}

	// Check the unit number
	if (un < 0 || un > 1) {
		cerr << __PRETTY_FUNCTION__ << " Error : Invalid UNIT " << un << endl;
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
		cerr << "Error : Invalid file path" << endl;
		exit(EINVAL);
	}

	// File check (command is INSERT)
	if (cmd == INSERT && file.empty()) {
		cerr << "Error : Invalid file path" << endl;
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

	// All done!
	exit(0);
}
