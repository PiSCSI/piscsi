//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rascsi_interface.pb.h"
#include "rascsi_exceptions.h"
#include "protobuf_connector.h"
#include <unistd.h>
#include <netinet/in.h>
#include <sstream>

using namespace std;
using namespace rascsi_interface;

int ProtobufConnector::ReadCommand(PbCommand& command, int socket)
{
	// Wait for connection
	sockaddr_in client;
	socklen_t socklen = sizeof(client);
	memset(&client, 0, socklen);
	int fd = accept(socket, (sockaddr*)&client, &socklen);
	if (fd < 0) {
		throw io_exception("accept() failed");
	}

	// Read magic string
	vector<byte> magic(6);
	size_t bytes_read = ReadBytes(fd, magic);
	if (!bytes_read) {
		return -1;
	}
	if (bytes_read != magic.size() || memcmp(magic.data(), "RASCSI", magic.size())) {
		throw io_exception("Invalid magic");
	}

	// Fetch the command
	DeserializeMessage(fd, command);

	return fd;
}

//---------------------------------------------------------------------------
//
//	Serialize/Deserialize protobuf message: Length followed by the actual data.
//  Little endian is assumed.
//
//---------------------------------------------------------------------------

void ProtobufConnector::SerializeMessage(int fd, const google::protobuf::Message& message)
{
	string data;
	message.SerializeToString(&data);

	// Write the size of the protobuf data as a header
	auto size = (int32_t)data.length();
    if (write(fd, &size, sizeof(size)) != sizeof(size)) {
    	throw io_exception("Can't write protobuf message header");
    }

    // Write the actual protobuf data
    if (write(fd, data.data(), size) != size) {
    	throw io_exception("Can't write protobuf message data");
    }
}

void ProtobufConnector::DeserializeMessage(int fd, google::protobuf::Message& message)
{
	// Read the header with the size of the protobuf data
	vector<byte> header_buf(4);
	if (ReadBytes(fd, header_buf) < header_buf.size()) {
		return;
	}

	size_t size = ((int)header_buf[3] << 24) + ((int)header_buf[2] << 16) + ((int)header_buf[1] << 8) + (int)header_buf[0];
	if (size <= 0) {
		throw io_exception("Invalid protobuf message header");
	}

	// Read the binary protobuf data
	vector<byte> data_buf(size);
	if (ReadBytes(fd, data_buf) < data_buf.size()) {
		throw io_exception("Missing protobuf message data");
	}

	// Create protobuf message
	string data((const char *)data_buf.data(), size);
	message.ParseFromString(data);
}

size_t ProtobufConnector::ReadBytes(int fd, vector<byte>& buf)
{
	size_t offset = 0;
	while (offset < buf.size()) {
		ssize_t len = read(fd, &buf[offset], buf.size() - offset);
		if (len <= 0) {
			return len;
		}

		offset += len;
	}

	return offset;
}
