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
	ctrl->length = ModeSense10(ctrl->cmd, ctrl->buffer, controller->DEFAULT_BUFFER_SIZE);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataIn();
}

bool ModePageDevice::ModeSelect(const DWORD* /*cdb*/, const BYTE *buf, int length)
{
	ASSERT(buf);
	ASSERT(length >= 0);

	// cannot be set
	SetStatusCode(STATUS_INVALIDPRM);

	return false;
}

void ModePageDevice::ModeSelect6(SASIDEV *controller)
{
	LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, ctrl->buffer[0]);

	ctrl->length = ModeSelectCheck6(ctrl->cmd);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataOut();
}

void ModePageDevice::ModeSelect10(SASIDEV *controller)
{
	LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, ctrl->buffer[0]);

	ctrl->length = ModeSelectCheck10(ctrl->cmd);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataOut();
}

int ModePageDevice::ModeSelectCheck(const DWORD *cdb, int length)
{
	// Error if save parameters are set for other types than of SCHD or SCRM
	// TODO This assumption is not correct, and this code should be located elsewhere
	if (!IsSCSIHD() && (cdb[1] & 0x01)) {
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}

	return length;
}

int ModePageDevice::ModeSelectCheck6(const DWORD *cdb)
{
	// Receive the data specified by the parameter length
	return ModeSelectCheck(cdb, cdb[4]);
}

int ModePageDevice::ModeSelectCheck10(const DWORD *cdb)
{
	// Receive the data specified by the parameter length
	int length = cdb[7];
	length <<= 8;
	length |= cdb[8];
	if (length > 0x800) {
		length = 0x800;
	}

	return ModeSelectCheck(cdb, length);
}
