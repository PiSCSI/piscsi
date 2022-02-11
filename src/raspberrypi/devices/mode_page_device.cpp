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

ModePageDevice::ModePageDevice(const string id) : PrimaryDevice(id)
{
	AddCommand(ScsiDefs::eCmdModeSense6, "ModeSense6", &ModePageDevice::ModeSense6);
	AddCommand(ScsiDefs::eCmdModeSense10, "ModeSense10", &ModePageDevice::ModeSense10);
	AddCommand(ScsiDefs::eCmdModeSelect6, "ModeSelect6", &ModePageDevice::ModeSelect6);
	AddCommand(ScsiDefs::eCmdModeSelect10, "ModeSelect10", &ModePageDevice::ModeSelect10);
}

void ModePageDevice::AddCommand(ScsiDefs::scsi_command opcode, const char* name, void (ModePageDevice::*execute)(SASIDEV *))
{
	commands[opcode] = new command_t(name, execute);
}

bool ModePageDevice::Dispatch(SCSIDEV *controller)
{
	ctrl = controller->GetCtrl();

	auto it = commands.find(static_cast<ScsiDefs::scsi_command>(ctrl->cmd[0]));
	if (it != commands.end()) {
		LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, it->second->name, (unsigned int)ctrl->cmd[0]);

		(this->*it->second->execute)(controller);

		return true;
	}

	// The base class handles the less specific commands
	return PrimaryDevice::Dispatch(controller);
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
	ctrl->length = ModeSense10(ctrl->cmd, ctrl->buffer);
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
