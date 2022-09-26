//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// A basic device with mode page support, to be used for subclassing
//
//---------------------------------------------------------------------------

#include "log.h"
#include "rascsi_exceptions.h"
#include "scsi_command_util.h"
#include "dispatcher.h"
#include "mode_page_device.h"
#include <cstddef>

using namespace std;
using namespace scsi_defs;
using namespace scsi_command_util;

ModePageDevice::ModePageDevice(const string& id) : PrimaryDevice(id)
{
	dispatcher.Add(scsi_command::eCmdModeSense6, "ModeSense6", &ModePageDevice::ModeSense6);
	dispatcher.Add(scsi_command::eCmdModeSense10, "ModeSense10", &ModePageDevice::ModeSense10);
	dispatcher.Add(scsi_command::eCmdModeSelect6, "ModeSelect6", &ModePageDevice::ModeSelect6);
	dispatcher.Add(scsi_command::eCmdModeSelect10, "ModeSelect10", &ModePageDevice::ModeSelect10);
}

bool ModePageDevice::Dispatch(scsi_command cmd)
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, cmd) ? true : super::Dispatch(cmd);
}

int ModePageDevice::AddModePages(const vector<int>& cdb, vector<BYTE>& buf, int offset, int max_length) const
{
	if (max_length < 0) {
		return 0;
	}

	bool changeable = (cdb[2] & 0xc0) == 0x40;

	// Get page code (0x3f means all pages)
	int page = cdb[2] & 0x3f;

	LOGTRACE("%s Requesting mode page $%02X", __PRETTY_FUNCTION__, page)

	// Mode page data mapped to the respective page numbers, C++ maps are ordered by key
	map<int, vector<byte>> pages;
	SetUpModePages(pages, page, changeable);

	if (pages.empty()) {
		LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, page)
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Holds all mode page data
	vector<byte> result;

	vector<byte> page0;
	for (auto const& [index, data] : pages) {
		// The specification mandates that page 0 must be returned after all others
		if (index) {
			// Page data
			result.insert(result.end(), data.begin(), data.end());
			// Page code, PS bit may already have been set
			result[result.size()] |= (byte)index;
			// Page payload size
			result[result.size() + 1] = (byte)(data.size() - 2);
		}
		else {
			page0 = data;
		}
	}

	// Page 0 must be last
	if (!page0.empty()) {
		// Page data
		result.insert(result.end(), page0.begin(), page0.end());
		// Page payload size
		result[result.size() + 1] = (byte)(page0.size() - 2);
	}

	// Do not return more than the requested number of bytes
	size_t size = min((size_t)max_length, result.size());
	memcpy(&buf.data()[offset], result.data(), size);

	return (int)size;
}

void ModePageDevice::ModeSense6()
{
	ctrl->length = ModeSense6(ctrl->cmd, controller->GetBuffer());

	EnterDataInPhase();
}

void ModePageDevice::ModeSense10()
{
	ctrl->length = ModeSense10(ctrl->cmd, controller->GetBuffer());

	EnterDataInPhase();
}

void ModePageDevice::ModeSelect(const vector<int>&, const vector<BYTE>&, int) const
{
	throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_COMMAND_OPERATION_CODE);
}

void ModePageDevice::ModeSelect6()
{
	ctrl->length = ModeSelectCheck6();

	EnterDataOutPhase();
}

void ModePageDevice::ModeSelect10()
{
	ctrl->length = ModeSelectCheck10();

	EnterDataOutPhase();
}

int ModePageDevice::ModeSelectCheck(int length) const
{
	// Error if save parameters are set for other types than SCHD, SCRM or SCMO
	// TODO The assumption above is not correct, and this code should be located elsewhere
	if (GetType() != "SCHD" && GetType() != "SCRM" && GetType() != "SCMO" && (ctrl->cmd[1] & 0x01)) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	return length;
}

int ModePageDevice::ModeSelectCheck6() const
{
	// Receive the data specified by the parameter length
	return ModeSelectCheck(ctrl->cmd[4]);
}

int ModePageDevice::ModeSelectCheck10() const
{
	// Receive the data specified by the parameter length
	size_t length = min(controller->GetBufferSize(), (size_t)GetInt16(ctrl->cmd, 7));

	return ModeSelectCheck((int)length);
}
