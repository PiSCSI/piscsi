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
#include "command_context.h"
#include <iostream>

using namespace std;
using namespace rascsi_interface;

void CommandContext::Cleanup() const
{
	if (fd != -1) {
		close(fd);
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
	LOGERROR("%s", localizer.Localize(key, "en", arg1, arg2, arg3).c_str())

	return ReturnStatus(false, localizer.Localize(key, locale, arg1, arg2, arg3), error_code, false);
}

bool CommandContext::ReturnStatus(bool status, const string& msg, PbErrorCode error_code, bool log) const
{
	// Do not log twice if logging has already been done in the localized error handling above
	if (log && !status && !msg.empty()) {
		LOGERROR("%s", msg.c_str())
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
