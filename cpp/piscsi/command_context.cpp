//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "command_context.h"
#include <spdlog/spdlog.h>
#include <iostream>

using namespace std;
using namespace piscsi_interface;

void CommandContext::Cleanup()
{
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
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
			if (status) {
				cerr << "Error: " << msg << endl;
			}
			else {
				cout << msg << endl;
			}
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

bool CommandContext::ReturnErrorStatus(const string& msg) const
{
	return ReturnStatus(false, msg, PbErrorCode::NO_ERROR_CODE, true);
}
