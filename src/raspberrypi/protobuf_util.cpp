//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "rascsi_interface.pb.h"
#include "localizer.h"
#include "rascsi_exceptions.h"
#include "protobuf_util.h"
#include <unistd.h>
#include <netinet/in.h>
#include <sstream>

using namespace std;
using namespace rascsi_interface;

#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )

static const char COMPONENT_SEPARATOR = ':';
static const char KEY_VALUE_SEPARATOR = '=';

const Localizer localizer;

void protobuf_util::ParseParameters(PbDeviceDefinition& device, const string& params)
{
	if (!params.empty()) {
		if (params.find(KEY_VALUE_SEPARATOR) != string::npos) {
			stringstream ss(params);
			string p;
			while (getline(ss, p, COMPONENT_SEPARATOR)) {
				if (!p.empty()) {
					size_t separator_pos = p.find(KEY_VALUE_SEPARATOR);
					if (separator_pos != string::npos) {
						AddParam(device, p.substr(0, separator_pos), string_view(p).substr(separator_pos + 1));
					}
				}
			}
		}
		// Old style parameters, for backwards compatibility only.
		// Only one of these parameters will be used by rascsi, depending on the device type.
		else {
			AddParam(device, "file", params);
			if (params != "bridge" && params != "daynaport" && params != "printer" && params != "services") {
				AddParam(device, "interfaces", params);
			}
		}
    }
}

string protobuf_util::GetParam(const PbCommand& command, const string& key)
{
	auto map = command.params();
	return map[key];
}

string protobuf_util::GetParam(const PbDeviceDefinition& device, const string& key)
{
	auto map = device.params();
	return map[key];
}

void protobuf_util::AddParam(PbCommand& command, const string& key, string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *command.mutable_params();
		map[key] = value;
	}
}

void protobuf_util::AddParam(PbDevice& device, const string& key,string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *device.mutable_params();
		map[key] = value;
	}
}

void protobuf_util::AddParam(PbDeviceDefinition& device, const string& key, string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *device.mutable_params();
		map[key] = value;
	}
}

bool protobuf_util::ReadCommand(PbCommand& command, CommandContext& context, int socket)
{
	// Wait for connection
	sockaddr_in client;
	socklen_t socklen = sizeof(client);
	memset(&client, 0, socklen);
	context.fd = accept(socket, (sockaddr*)&client, &socklen);
	if (context.fd < 0) {
		throw io_exception("accept() failed");
	}

	// Read magic string
	vector<byte> magic(6);
	size_t bytes_read = ReadBytes(context.fd, magic);
	if (!bytes_read) {
		return false;
	}
	if (bytes_read != magic.size() || memcmp(magic.data(), "RASCSI", magic.size())) {
		throw io_exception("Invalid magic");
	}

	// Fetch the command
	DeserializeMessage(context.fd, command);

	return true;
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
	auto size = (int32_t)data.length();
    if (write(fd, &size, sizeof(size)) != sizeof(size)) {
    	throw io_exception("Can't write protobuf message header");
    }

    // Write the actual protobuf data
    if (write(fd, data.data(), size) != size) {
    	throw io_exception("Can't write protobuf message data");
    }
}

void protobuf_util::DeserializeMessage(int fd, google::protobuf::Message& message)
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

size_t protobuf_util::ReadBytes(int fd, vector<byte>& buf)
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

bool protobuf_util::ReturnLocalizedError(const CommandContext& context, const LocalizationKey key,
		const string& arg1, const string& arg2, const string& arg3)
{
	return ReturnLocalizedError(context, key, NO_ERROR_CODE, arg1, arg2, arg3);
}

bool protobuf_util::ReturnLocalizedError(const CommandContext& context, const LocalizationKey key,
		const PbErrorCode error_code, const string& arg1, const string& arg2, const string& arg3)
{
	// For the logfile always use English
	LOGERROR("%s", localizer.Localize(key, "en", arg1, arg2, arg3).c_str())

	return ReturnStatus(context, false, localizer.Localize(key, context.locale, arg1, arg2, arg3), error_code, false);
}

bool protobuf_util::ReturnStatus(const CommandContext& context, bool status, const string& msg,
		const PbErrorCode error_code, bool log)
{
	// Do not log twice if logging has already been done in the localized error handling above
	if (log && !status && !msg.empty()) {
		LOGERROR("%s", msg.c_str())
	}

	if (context.fd == -1) {
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
		result.set_error_code(error_code);
		result.set_msg(msg);
		SerializeMessage(context.fd, result);
	}

	return status;
}
