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
#include <list>

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
    		ostringstream error;
    		error << "Can't connect to rascsi process on host '" << hostname << "', port " << port;
    		throw ioexception(error.str());
    	}

        SerializeMessage(fd, command);
    }
    catch(const ioexception& e) {
    	cerr << "Error: " << e.getmsg() << endl;

        if (fd >= 0) {
        	close(fd);
        }

        exit(fd < 0 ? ENOTCONN : -1);
    }

    return fd;
}

//---------------------------------------------------------------------------
//
//	Receive command result
//
//---------------------------------------------------------------------------
bool ReceiveResult(int fd)
{
    try {
        PbResult result;
        DeserializeMessage(fd, result);
        close(fd);

    	if (!result.status()) {
    		throw ioexception(result.msg());
    	}

    	cout << result.msg() << endl;
    }
    catch(const ioexception& e) {
    	cerr << "Error: " << e.getmsg() << endl;

    	return false;
    }

    return true;
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
	ReceiveResult(fd);
	close(fd);
}

void CommandServerInfo(const string& hostname, int port)
{
	PbCommand command;
	command.set_cmd(SERVER_INFO);

	int fd = SendCommand(hostname.c_str(), port, command);

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

	cout << "rascsi server version: " << serverInfo.rascsi_version() << endl;

	if (!serverInfo.available_log_levels_size()) {
		cout << "  No log level settings available" << endl;
	}
	else {
		cout << "Available rascsi log levels, sorted by severity:" << endl;
		for (int i = 0; i < serverInfo.available_log_levels_size(); i++) {
			cout << "  " << serverInfo.available_log_levels(i) << endl;
		}

		cout << "Current rascsi log level: " << serverInfo.current_log_level() << endl;
	}

	cout << "Default image file folder: " << serverInfo.default_image_folder() << endl;
	if (!serverInfo.available_image_files_size()) {
		cout << "  No image files available in the default folder" << endl;
	}
	else {
		list<string> sorted_image_files;
		for (int i = 0; i < serverInfo.available_image_files_size(); i++) {
			sorted_image_files.push_back(serverInfo.available_image_files(i));
		}
		sorted_image_files.sort();

		cout << "Image files available in the default folder:" << endl;
		for (auto it = sorted_image_files.begin(); it != sorted_image_files.end(); ++it) {
			cout << "  " << *it << endl;
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
		cerr << "Usage: " << argv[0] << " -i ID [-u UNIT] [-c CMD] [-t TYPE] [-f FILE] [-g LOG_LEVEL] [-h HOST] [-p PORT] [-v]" << endl;
		cerr << " where  ID := {0|1|2|3|4|5|6|7}" << endl;
		cerr << "        UNIT := {0|1} default setting is 0." << endl;
		cerr << "        CMD := {attach|detach|insert|eject|protect|unprotect}" << endl;
		cerr << "        TYPE := {hd|mo|cd|bridge|daynaport}" << endl;
		cerr << "        FILE := image file path" << endl;
		cerr << "        HOST := rascsi host to connect to, default is 'localhost'" << endl;
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
	while ((opt = getopt(argc, argv, "i:u:c:t:f:h:p:u:g:lsv")) != -1) {
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

					case 'u':
						cmd = UNPROTECT;
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

			case 'v':
				cout << rascsi_get_version_string() << endl;
				exit(0);
				break;
		}
	}

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

	// Generate the command and send it
	PbCommand command;
	command.set_id(id);
	command.set_un(un);
	command.set_cmd(cmd);
	command.set_type(type);
	if (!params.empty()) {
		command.set_params(params);
	}

	int fd = SendCommand(hostname, port, command);
	bool status = ReceiveResult(fd);
	close(fd);

	// All done!
	exit(status ? 0 : -1);
}
