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
#include "exceptions.h"
#include "mode_page_device.h"

using namespace std;
using namespace scsi_defs;

ModePageDevice::ModePageDevice(const string& id) : PrimaryDevice(id)
{
	dispatcher.AddCommand(eCmdModeSense6, "ModeSense6", &ModePageDevice::ModeSense6);
	dispatcher.AddCommand(eCmdModeSense10, "ModeSense10", &ModePageDevice::ModeSense10);
	dispatcher.AddCommand(eCmdModeSelect6, "ModeSelect6", &ModePageDevice::ModeSelect6);
	dispatcher.AddCommand(eCmdModeSelect10, "ModeSelect10", &ModePageDevice::ModeSelect10);
}

bool ModePageDevice::Dispatch()
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, ctrl->cmd[0]) ? true : super::Dispatch();
}

int ModePageDevice::AddModePages(const DWORD *cdb, BYTE *buf, int max_length)
{
	if (max_length <= 0) {
		return 0;
	}

	bool changeable = (cdb[2] & 0xc0) == 0x40;

	// Get page code (0x3f means all pages)
	int page = cdb[2] & 0x3f;

	LOGTRACE("%s Requesting mode page $%02X", __PRETTY_FUNCTION__, page);

	// Mode page data mapped to the respective page numbers, C++ maps are ordered by key
	map<int, vector<BYTE>> pages;
	AddModePages(pages, page, changeable);

	if (pages.empty()) {
		LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, page);
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Holds all mode page data
	vector<BYTE> result;

	vector<BYTE> page0;
	for (auto const& page : pages) {
		// The specification mandates that page 0 must be returned after all others
		if (page.first) {
			size_t offset = result.size();

			// Page data
			result.insert(result.end(), page.second.begin(), page.second.end());
			// Page code, PS bit may already have been set
			result[offset] |= page.first;
			// Page payload size
			result[offset + 1] = page.second.size() - 2;
		}
		else {
			page0 = page.second;
		}
	}

	// Page 0 must be last
	if (!page0.empty()) {
		size_t offset = result.size();

		// Page data
		result.insert(result.end(), page0.begin(), page0.end());
		// Page payload size
		result[offset + 1] = page0.size() - 2;
	}

	// Do not return more than the requested number of bytes
	size_t size = (size_t)max_length < result.size() ? max_length : result.size();
	memcpy(buf, result.data(), size);

	return size;
}

void ModePageDevice::ModeSense6()
{
	ctrl->length = ModeSense6(ctrl->cmd, ctrl->buffer, ctrl->bufsize);

	EnterDataInPhase();
}

void ModePageDevice::ModeSense10()
{
	ctrl->length = ModeSense10(ctrl->cmd, ctrl->buffer, ctrl->bufsize);

	EnterDataInPhase();
}

void ModePageDevice::ModeSelect(const DWORD*, const BYTE *, int)
{
	throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_COMMAND_OPERATION_CODE);
}

void ModePageDevice::ModeSelect6()
{
	LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, ctrl->buffer[0]);

	ctrl->length = ModeSelectCheck6();

	EnterDataOutPhase();
}

void ModePageDevice::ModeSelect10()
{
	LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, ctrl->buffer[0]);

	ctrl->length = ModeSelectCheck10();

	EnterDataOutPhase();
}

int ModePageDevice::ModeSelectCheck(int length)
{
	// Error if save parameters are set for other types than of SCHD, SCRM or SCMO
	// TODO The assumption above is not correct, and this code should be located elsewhere
	if (GetType() != "SCHD" && GetType() != "SCRM" && GetType() != "SCMO" && (ctrl->cmd[1] & 0x01)) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	return length;
}

int ModePageDevice::ModeSelectCheck6()
{
	// Receive the data specified by the parameter length
	return ModeSelectCheck(ctrl->cmd[4]);
}

int ModePageDevice::ModeSelectCheck10()
{
	// Receive the data specified by the parameter length
	int length = ctrl->cmd[7];
	length <<= 8;
	length |= ctrl->cmd[8];
	if (length > ctrl->bufsize) {
		length = ctrl->bufsize;
	}

	return ModeSelectCheck(length);
}
