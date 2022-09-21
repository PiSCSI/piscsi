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
#include "protobuf_connector.h"
#include "command_util.h"
#include <unistd.h>
#include <netinet/in.h>
#include <sstream>

using namespace std;
using namespace rascsi_interface;

#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )

static const char COMPONENT_SEPARATOR = ':';
static const char KEY_VALUE_SEPARATOR = '=';

const Localizer localizer;

extern ProtobufConnector protobuf_connector;

void command_util::ParseParameters(PbDeviceDefinition& device, const string& params)
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

string command_util::GetParam(const PbCommand& command, const string& key)
{
	auto map = command.params();
	return map[key];
}

string command_util::GetParam(const PbDeviceDefinition& device, const string& key)
{
	auto map = device.params();
	return map[key];
}

void command_util::AddParam(PbCommand& command, const string& key, string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *command.mutable_params();
		map[key] = value;
	}
}

void command_util::AddParam(PbDevice& device, const string& key,string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *device.mutable_params();
		map[key] = value;
	}
}

void command_util::AddParam(PbDeviceDefinition& device, const string& key, string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *device.mutable_params();
		map[key] = value;
	}
}

bool command_util::ReturnLocalizedError(const CommandContext& context, const LocalizationKey key,
		const string& arg1, const string& arg2, const string& arg3)
{
	return ReturnLocalizedError(context, key, NO_ERROR_CODE, arg1, arg2, arg3);
}

bool command_util::ReturnLocalizedError(const CommandContext& context, const LocalizationKey key,
		const PbErrorCode error_code, const string& arg1, const string& arg2, const string& arg3)
{
	// For the logfile always use English
	LOGERROR("%s", localizer.Localize(key, "en", arg1, arg2, arg3).c_str())

	return ReturnStatus(context, false, localizer.Localize(key, context.locale, arg1, arg2, arg3), error_code, false);
}

bool command_util::ReturnStatus(const CommandContext& context, bool status, const string& msg,
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
		protobuf_connector.SerializeMessage(context.fd, result);
	}

	return status;
}
