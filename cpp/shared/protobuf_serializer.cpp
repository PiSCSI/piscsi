//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/protobuf_serializer.h"
#include "shared/piscsi_exceptions.h"
#include "generated/piscsi_interface.pb.h"
#include <unistd.h>

using namespace std;
using namespace piscsi_interface;

//---------------------------------------------------------------------------
//
// Serialize/Deserialize protobuf message: Length followed by the actual data.
// A little endian platform is assumed.
//
//---------------------------------------------------------------------------

void ProtobufSerializer::SerializeMessage(int fd, const google::protobuf::Message& message) const
{
	string data;
	message.SerializeToString(&data);

	// Write the size of the protobuf data as a header
	const auto size = static_cast<int32_t>(data.length());
    if (write(fd, &size, sizeof(size)) != sizeof(size)) {
    	throw io_exception("Can't write protobuf message size");
    }

    // Write the actual protobuf data
    if (write(fd, data.data(), size) != size) {
    	throw io_exception("Can't write protobuf message data");
    }
}

void ProtobufSerializer::DeserializeMessage(int fd, google::protobuf::Message& message) const
{
	// Read the header with the size of the protobuf data
	array<byte, sizeof(int32_t)> header_buf;
	if (ReadBytes(fd, header_buf) < header_buf.size()) {
		throw io_exception("Can't read protobuf message size");
	}

	const int size = (static_cast<int>(header_buf[3]) << 24) + (static_cast<int>(header_buf[2]) << 16)
			+ (static_cast<int>(header_buf[1]) << 8) + static_cast<int>(header_buf[0]);
	if (size < 0) {
		throw io_exception("Invalid protobuf message size");
	}

	// Read the binary protobuf data
	vector<byte> data_buf(size);
	if (ReadBytes(fd, data_buf) != data_buf.size()) {
		throw io_exception("Missing protobuf message data");
	}

	// Create protobuf message
	const string data((const char *)data_buf.data(), size);
	message.ParseFromString(data);
}

size_t ProtobufSerializer::ReadBytes(int fd, span<byte> buf) const
{
	size_t offset = 0;
	while (offset < buf.size()) {
		const auto len = read(fd, &buf.data()[offset], buf.size() - offset);
		if (len == -1) {
			throw io_exception("Read error: " + string(strerror(errno)));
		}

		if (!len) {
			break;
		}

		offset += len;
	}

	return offset;
}
