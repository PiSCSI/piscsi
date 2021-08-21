//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <unistd.h>
#include <sstream>
#include "rascsi_interface.pb.h"
#include "exceptions.h"
#include "protobuf_util.h"

using namespace std;
using namespace rascsi_interface;

//---------------------------------------------------------------------------
//
//	Serialize/Deserialize protobuf message: Length followed by the actual data.
//  Little endian is assumed.
//
//---------------------------------------------------------------------------

void SerializeMessage(int fd, const google::protobuf::MessageLite& message)
{
	string data;
	message.SerializeToString(&data);

	// Write the size of the protobuf data as a header
    int32_t size = data.length();
    if (write(fd, &size, sizeof(size)) != sizeof(size)) {
    	throw io_exception("Can't write protobuf header");
    }

    // Write the actual protobuf data
    uint8_t buf[size];
    memcpy(buf, data.data(), size);
    if (write(fd, buf, size) != size) {
    	throw io_exception("Can't write protobuf data");
    }
}

void DeserializeMessage(int fd, google::protobuf::MessageLite& message)
{
	// Read the header with the size of the protobuf data
	uint8_t header_buf[4];
	int bytes_read = ReadNBytes(fd, header_buf, 4);
	if (bytes_read < 4) {
		return;
	}
	int32_t size = (header_buf[3] << 24) + (header_buf[2] << 16) + (header_buf[1] << 8) + header_buf[0];

	// Read the binary protobuf data
	uint8_t data_buf[size];
	bytes_read = ReadNBytes(fd, data_buf, size);
	if (bytes_read < size) {
		throw io_exception("Missing protobuf data");
	}

	// Create protobuf message
	string data((const char *)data_buf, size);
	message.ParseFromString(data);
}

int ReadNBytes(int fd, uint8_t *buf, int n)
{
	int offset = 0;
	while (offset < n) {
		ssize_t len = read(fd, buf + offset, n - offset);
		if (!len) {
			break;
		}

		offset += len;
	}

	return offset;
}
