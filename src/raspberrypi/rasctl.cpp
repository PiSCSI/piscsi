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

#include <netdb.h>
#include "google/protobuf/message_lite.h"
#include "os.h"
#include "rascsi_version.h"
#include "exceptions.h"
#include "rasutil.h"
#include "rascsi_interface.pb.h"
#include <sstream>
#include <iostream>

using namespace std;
using namespace rascsi_interface;

//---------------------------------------------------------------------------
//
//	Send Command
//
//---------------------------------------------------------------------------
int SendCommand(const string& hostname, int port, const PbCommand& command)
{
	int fd = -1;

	try {
    	struct hostent *host = gethostbyname(hostname.c_str());
    	if (!host) {
    		throw ioexception("Can't resolve hostname '" + hostname + "'");
    	}

    	fd = socket(AF_INET, SOCK_STREAM, 0);
    	if (fd < 0) {
    		throw ioexception("Can't create socket");
    	}

    	struct sockaddr_in server;
    	memset(&server, 0, sizeof(server));
    	server.sin_family = AF_INET;
    	server.sin_port = htons(port);
    	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    	memcpy(&server.sin_addr.s_addr, host->h_addr, host->h_length);

    	if (connect(fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
    		ostringstream s;
    		s << "Can't connect to rascsi process on host '" << hostname << "', port " << port;
    		throw ioexception(s.str());
    	}

        SerializeMessage(fd, command);
    }
    catch(const ioexception& e) {
    	cerr << "Error: " << e.getmsg() << endl;

        if (fd >= 0) {
        	close(fd);
        }

        return -1;
    }

    return fd;
}

//---------------------------------------------------------------------------
//
//	Receive command result
//
//---------------------------------------------------------------------------
bool ReceiveResult(int fd) {
    bool status = true;

    try {
        PbResult result;
        DeserializeMessage(fd, result);

        status = result.status();
    	if (status) {
    		cerr << result.msg();
    	}
    	else {
    		cout << result.msg();
    	}
    }
    catch(const ioexception& e) {
    	cerr << "Error: " << e.getmsg() << endl;

    	// Fall through

    	status = false;
    }

    close(fd);

    return status;
}

//---------------------------------------------------------------------------
//
//	Command implementations
//
//---------------------------------------------------------------------------

void CommandList(const string& hostname, int port)
{
	PbCommand command;
	command.set_cmd(LIST);

	int fd = SendCommand(hostname.c_str(), port, command);
	if (fd < 0) {
		exit(ENOTCONN);
	}

	PbDevices devices;
	try {
		DeserializeMessage(fd, devices);
	}
	catch(const ioexception& e) {
		cerr << "Error: " << e.getmsg() << endl;

		close(fd);

		exit(-1);
	}

	close (fd);

	cout << ListDevices(devices) << endl;
}

void CommandLogLevel(const string& hostname, int port, const string& log_level)
{
	PbCommand command;
	command.set_cmd(LOG_LEVEL);
	command.set_params(log_level);

	int fd = SendCommand(hostname.c_str(), port, command);
	if (fd < 0) {
		exit(ENOTCONN);
	}

	ReceiveResult(fd);

	close(fd);
}

void CommandServerInfo(const string& hostname, int port)
{
	PbCommand command;
	command.set_cmd(SERVER_INFO);

	int fd = SendCommand(hostname.c_str(), port, command);
	if (fd < 0) {
		exit(ENOTCONN);
	}


	PbServerInfo serverInfo;
	try {
		DeserializeMessage(fd, serverInfo);
	}
	catch(const ioexception& e) {
		cerr << "Error: " << e.getmsg() << endl;

		close(fd);

		exit(-1);
	}

	close(fd);

	cout << "rascsi version: " << serverInfo.rascsi_version() << endl;
	cout << "rascsi log level: " << serverInfo.log_level() << endl;
	cout << "Default image file folder: " << serverInfo.default_image_folder() << endl;
	if (!serverInfo.available_images_size()) {
		cout << "No image files available" << endl;
	}
	else {
		cout << "Available image files:" << endl;
		for (int i = 0; i < serverInfo.available_images_size(); i++) {
			cout << "  " << serverInfo.available_images(i) << endl;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// Display help
	if (argc < 2) {
		cerr << "SCSI Target Emulator RaSCSI Controller" << endl;
		cerr << "version " << rascsi_get_version_string() << " (" << __DATE__ << ", " << __TIME__ << ")" << endl;
		cerr << "Usage: " << argv[0] << " -i ID [-u UNIT] [-c CMD] [-t TYPE] [-f FILE] [-h HOSTNAME] [-p PORT] [-g LOG_LEVEL] [-s]" << endl;
		cerr << " where  ID := {0|1|2|3|4|5|6|7}" << endl;
		cerr << "        UNIT := {0|1} default setting is 0." << endl;
		cerr << "        CMD := {attach|detach|insert|eject|protect}" << endl;
		cerr << "        TYPE := {hd|mo|cd|bridge|daynaport}" << endl;
		cerr << "        FILE := image file path" << endl;
		cerr << "        HOSTNAME := rascsi host to connect to, default is 'localhost'" << endl;
		cerr << "        PORT := rascsi port to connect to, default is 6868" << endl;
		cerr << "        LOG_LEVEL := log level {trace|debug|info|warn|err|critical|off}, default is 'trace'" << endl;
		cerr << " If CMD is 'attach' or 'insert' the FILE parameter is required." << endl;
		cerr << "Usage: " << argv[0] << " -l" << endl;
		cerr << "       Print device list." << endl;

		exit(0);
	}

	// Parse the arguments
	int opt;
	int id = -1;
	int un = 0;
	PbOperation cmd = LIST;
	PbDeviceType type = UNDEFINED;
	const char *hostname = "localhost";
	int port = 6868;
	string params;
	opterr = 0;
	while ((opt = getopt(argc, argv, "i:u:c:t:f:h:p:g:ls")) != -1) {
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
				params = optarg;
				break;

			case 'l':
				cmd = LIST;
				break;

			case 'h':
				hostname = optarg;
				break;

			case 'p':
				port = atoi(optarg);
				if (port <= 0 || port > 65535) {
					cerr << "Invalid port " << optarg << ", port must be between 1 and 65535" << endl;
					exit(-1);
				}
				break;

			case 'g':
				cmd = LOG_LEVEL;
				params = optarg;
				break;

			case 's':
				cmd = SERVER_INFO;
				break;
		}
	}

	PbCommand command;

	if (cmd == LOG_LEVEL) {
		CommandLogLevel(hostname, port, params);
		exit(0);
	}

	if (cmd == SERVER_INFO) {
		CommandServerInfo(hostname, port);
		exit(0);
	}

	// List display only
	if (cmd == LIST || (id < 0 && type == UNDEFINED && params.empty())) {
		CommandList(hostname, port);
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
		int len = params.length();
		if (len > 4 && params[len - 4] == '.') {
			string ext = params.substr(len - 3);
			if (ext == "hdf" || ext == "hds" || ext == "hdn" || ext == "hdi" || ext == "nhd" || ext == "hda") {
				type = SASI_HD;
			} else if (ext == "mos") {
				type = MO;
			} else if (ext == "iso") {
				type = CD;
			}
		}
	}

	// File check (command is ATTACH and type is HD, for CD and MO the medium (=file) may be inserted later)
	if (cmd == ATTACH && (type == SASI_HD || type == SCSI_HD) && params.empty()) {
		cerr << "Error : Invalid file path" << endl;
		exit(EINVAL);
	}

	// File check (command is INSERT)
	if (cmd == INSERT && params.empty()) {
		cerr << "Error : Invalid file path" << endl;
		exit(EINVAL);
	}

	// Generate the command and send it
	command.set_id(id);
	command.set_un(un);
	command.set_cmd(cmd);
	command.set_type(type);
	if (!params.empty()) {
		command.set_params(params);
	}

	int fd = SendCommand(hostname, port, command);
	if (fd < 0) {
		exit(ENOTCONN);
	}

	bool status = ReceiveResult(fd);
	close(fd);

	// All done!
	exit(status ? 0 : -1);
}
