//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <unistd.h>
#include "os.h"
#include "log.h"
#include "rascsi_interface.pb.h"
#include "exceptions.h"
#include "protobuf_util.h"

using namespace std;
using namespace rascsi_interface;

#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )

const string protobuf_util::GetParam(const PbCommand& command, const string& key)
{
	auto map = command.params();
	return map[key];
}

const string protobuf_util::GetParam(const PbDeviceDefinition& device, const string& key)
{
	auto map = device.params();
	return map[key];
}

void protobuf_util::AddParam(PbCommand& command, const string& key, const string& value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *command.mutable_params();
		map[key] = value;
	}
}

void protobuf_util::AddParam(PbDevice& device, const string& key, const string& value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *device.mutable_params();
		map[key] = value;
	}
}

void protobuf_util::AddParam(PbDeviceDefinition& device, const string& key, const string& value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *device.mutable_params();
		map[key] = value;
	}
}

//---------------------------------------------------------------------------
//
//	Serialize/Deserialize protobuf message: Length followed by the actual data.
//  Little endian is assumed.
//
//---------------------------------------------------------------------------

void protobuf_util::SerializeMessage(int fd, const google::protobuf::Message& message)
{
	string data;
	message.SerializeToString(&data);

	// Write the size of the protobuf data as a header
    int32_t size = data.length();
    if (write(fd, &size, sizeof(size)) != sizeof(size)) {
    	throw io_exception("Can't write protobuf header");
    }

    // Write the actual protobuf data
    if (write(fd, data.data(), size) != size) {
    	throw io_exception("Can't write protobuf data");
    }
}

void protobuf_util::DeserializeMessage(int fd, google::protobuf::Message& message)
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

int protobuf_util::ReadNBytes(int fd, uint8_t *buf, int n)
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


bool protobuf_util::ReturnStatus(int fd, bool status, const string msg)
{
	if (!status && !msg.empty()) {
		LOGERROR("%s", msg.c_str());
	}

	if (fd == -1) {
		if (!msg.empty()) {
			if (status) {
				FPRT(stderr, "Error: ");
				FPRT(stderr, "%s", msg.c_str());
				FPRT(stderr, "\n");
			}
			else {
				FPRT(stdout, "%s", msg.c_str());
				FPRT(stderr, "\n");
			}
		}
	}
	else {
		PbResult result;
		result.set_status(status);
		result.set_msg(msg);
		SerializeMessage(fd, result);
	}

	return status;
}

bool protobuf_util::ReturnStatus(int fd, bool status, const ostringstream& msg)
{
	return ReturnStatus(fd, status, msg.str());
}
