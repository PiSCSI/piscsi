//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "controllers/scsidev_ctrl.h"
#include "primary_device.h"

using namespace std;

PrimaryDevice::PrimaryDevice(const string id) : ScsiPrimaryCommands(), Device(id)
{
	ctrl = NULL;

	AddCommand(ScsiDefs::eCmdTestUnitReady, "TestUnitReady", &PrimaryDevice::TestUnitReady);
	AddCommand(ScsiDefs::eCmdInquiry, "Inquiry", &PrimaryDevice::Inquiry);
	AddCommand(ScsiDefs::eCmdReportLuns, "ReportLuns", &PrimaryDevice::ReportLuns);
}

void PrimaryDevice::AddCommand(ScsiDefs::scsi_command opcode, const char* name, void (PrimaryDevice::*execute)(SASIDEV *))
{
	commands[opcode] = new command_t(name, execute);
}

bool PrimaryDevice::Dispatch(SCSIDEV *controller)
{
	ctrl = controller->GetCtrl();

	if (commands.count(static_cast<ScsiDefs::scsi_command>(ctrl->cmd[0]))) {
		command_t *command = commands[static_cast<ScsiDefs::scsi_command>(ctrl->cmd[0])];

		LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, command->name, (unsigned int)ctrl->cmd[0]);

		(this->*command->execute)(controller);

		return true;
	}

	// Unknown command
	return false;
}

void PrimaryDevice::TestUnitReady(SASIDEV *controller)
{
	if (!CheckReady()) {
		controller->Error();
		return;
	}

	controller->Status();
}

void PrimaryDevice::Inquiry(SASIDEV *controller)
{
	int lun = controller->GetEffectiveLun();
	const Device *device = ctrl->unit[lun];

	// Find a valid unit
	// TODO The code below is probably wrong. It results in the same INQUIRY data being
	// used for all LUNs, even though each LUN has its individual set of INQUIRY data.
	// In addition, it supports gaps in the LUN list, which is not correct.
	if (!device) {
		for (int valid_lun = 0; valid_lun < SASIDEV::UnitMax; valid_lun++) {
			if (ctrl->unit[valid_lun]) {
				device = ctrl->unit[valid_lun];
				break;
			}
		}
	}

	if (device) {
		ctrl->length = Inquiry(ctrl->cmd, ctrl->buffer);
	} else {
		ctrl->length = 0;
	}

	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	// Report if the device does not support the requested LUN
	if (!ctrl->unit[lun]) {
		LOGDEBUG("Reporting LUN %d for device ID %d as not supported", lun, ctrl->device->GetId());

		ctrl->buffer[0] |= 0x7f;
	}

	controller->DataIn();
}

void PrimaryDevice::ReportLuns(SASIDEV *controller)
{
	BYTE *buf = ctrl->buffer;

	if (!CheckReady()) {
		controller->Error();
		return;
	}

	int allocation_length = (ctrl->cmd[6] << 24) + (ctrl->cmd[7] << 16) + (ctrl->cmd[8] << 8) + ctrl->cmd[9];
	memset(buf, 0, allocation_length);

	// Count number of available LUNs for the current device
	int luns;
	for (luns = 0; luns < controller->GetCtrl()->device->GetSupportedLuns(); luns++) {
		if (!controller->GetCtrl()->unit[luns]) {
			break;
		}
	}

	// LUN list length, 8 bytes per LUN
	// SCSI standard: The contents of the LUN LIST LENGTH field	are not altered based on the allocation length
	buf[0] = (luns * 8) >> 24;
	buf[1] = (luns * 8) >> 16;
	buf[2] = (luns * 8) >> 8;
	buf[3] = luns * 8;

	ctrl->length = allocation_length < 8 + luns * 8 ? allocation_length : 8 + luns * 8;

	controller->DataIn();
}

bool PrimaryDevice::CheckReady()
{
	// Not ready if reset
	if (IsReset()) {
		SetStatusCode(STATUS_DEVRESET);
		SetReset(false);
		LOGTRACE("%s Disk in reset", __PRETTY_FUNCTION__);
		return false;
	}

	// Not ready if it needs attention
	if (IsAttn()) {
		SetStatusCode(STATUS_ATTENTION);
		SetAttn(false);
		LOGTRACE("%s Disk in needs attention", __PRETTY_FUNCTION__);
		return false;
	}

	// Return status if not ready
	if (!IsReady()) {
		SetStatusCode(STATUS_NOTREADY);
		LOGTRACE("%s Disk not ready", __PRETTY_FUNCTION__);
		return false;
	}

	// Initialization with no error
	LOGTRACE("%s Disk is ready", __PRETTY_FUNCTION__);

	return true;
}
