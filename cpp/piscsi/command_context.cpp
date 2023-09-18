//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "command_context.h"
#include <spdlog/spdlog.h>
#include <iostream>

using namespace std;
using namespace piscsi_interface;

bool CommandContext::ReadCommand()
{
	// Read magic string
	array<byte, 6> magic;
	const size_t bytes_read = serializer.ReadBytes(fd, magic);
	if (bytes_read) {
		if (bytes_read != magic.size() || memcmp(magic.data(), "RASCSI", magic.size())) {
			throw io_exception("Invalid magic");
		}

		// Fetch the command
		serializer.DeserializeMessage(fd, command);

		return true;
	}

	return false;
}

bool CommandContext::ReturnLocalizedError(LocalizationKey key, const string& arg1, const string& arg2,
		const string& arg3) const
{
	return ReturnLocalizedError(key, NO_ERROR_CODE, arg1, arg2, arg3);
}

bool CommandContext::ReturnLocalizedError(LocalizationKey key, PbErrorCode error_code, const string& arg1,
		const string& arg2, const string& arg3) const
{
	// For the logfile always use English
	spdlog::error(localizer.Localize(key, "en", arg1, arg2, arg3));

	return ReturnStatus(false, localizer.Localize(key, locale, arg1, arg2, arg3), error_code, false);
}

bool CommandContext::ReturnStatus(bool status, const string& msg, PbErrorCode error_code, bool log) const
{
	// Do not log twice if logging has already been done in the localized error handling above
	if (log && !status && !msg.empty()) {
		spdlog::error(msg);
	}

	if (fd == -1) {
		if (!msg.empty()) {
			cerr << "Error: " << msg << endl;
		}
	}
	else {
		PbResult result;
		result.set_status(status);
		result.set_error_code(error_code);
		result.set_msg(msg);
		serializer.SerializeMessage(fd, result);
	}

	return status;
}

bool CommandContext::ReturnSuccessStatus() const
{
	return ReturnStatus(true, "", PbErrorCode::NO_ERROR_CODE, true);
}

bool CommandContext::ReturnErrorStatus(const string& msg) const
{
	return ReturnStatus(false, msg, PbErrorCode::NO_ERROR_CODE, true);
}
