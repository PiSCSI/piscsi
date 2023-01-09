//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// A basic device with mode page support, to be used for subclassing
//
//---------------------------------------------------------------------------

#include "log.h"
#include "controllers/scsidev_ctrl.h"
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

bool ModePageDevice::Dispatch(SCSIDEV *controller)
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, controller) ? true : super::Dispatch(controller);
}

int ModePageDevice::AddModePages(const DWORD *cdb, BYTE *buf, int max_length)
{
	bool changeable = (cdb[2] & 0xc0) == 0x40;

	// Get page code (0x3f means all pages)
	int page = cdb[2] & 0x3f;

	LOGTRACE("%s Requesting mode page $%02X", __PRETTY_FUNCTION__, page);

	// Mode page data mapped to the respective page numbers, C++ maps are ordered by key
	map<int, vector<BYTE>> pages;
	AddModePages(pages, page, changeable);

	// If no mode data were added at all something must be wrong
	if (pages.empty()) {
		LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, page);
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}

	int size = 0;

	vector<BYTE> page0;
	for (auto const& page : pages) {
		if (size + (int)page.second.size() > max_length) {
			LOGWARN("Mode page data size exceeds reserved buffer size");

			page0.clear();

			break;
		}
		else {
			// The specification mandates that page 0 must be returned after all others
			if (page.first) {
				// Page data
				memcpy(&buf[size], page.second.data(), page.second.size());
				// Page code, PS bit may already have been set
				buf[size] |= page.first;
				// Page payload size
				buf[size + 1] = page.second.size() - 2;

				size += page.second.size();
			}
			else {
				page0 = page.second;
			}
		}
	}

	// Page 0 must be last
	if (!page0.empty()) {
		memcpy(&buf[size], page0.data(), page0.size());
		// Page payload size
		buf[size + 1] = page0.size() - 2;
		size += page0.size();
	}

	return size;
}

void ModePageDevice::ModeSense6(SASIDEV *controller)
{
	ctrl->length = ModeSense6(ctrl->cmd, ctrl->buffer);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataIn();
}

void ModePageDevice::ModeSense10(SASIDEV *controller)
{
	ctrl->length = ModeSense10(ctrl->cmd, ctrl->buffer, ctrl->bufsize);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataIn();
}

bool ModePageDevice::ModeSelect(const DWORD*, const BYTE *, int)
{
	// Cannot be set
	SetStatusCode(STATUS_INVALIDPRM);

	return false;
}

void ModePageDevice::ModeSelect6(SASIDEV *controller)
{
	LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, ctrl->buffer[0]);

	ctrl->length = ModeSelectCheck6();
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataOut();
}

void ModePageDevice::ModeSelect10(SASIDEV *controller)
{
	LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, ctrl->buffer[0]);

	ctrl->length = ModeSelectCheck10();
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataOut();
}

int ModePageDevice::ModeSelectCheck(int length)
{
	// Error if save parameters are set for other types than of SCHD or SCRM
	// TODO The assumption above is not correct, and this code should be located elsewhere
	if (!IsSCSIHD() && (ctrl->cmd[1] & 0x01)) {
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
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
