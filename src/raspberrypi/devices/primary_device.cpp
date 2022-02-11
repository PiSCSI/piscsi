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
using namespace scsi_defs;

PrimaryDevice::PrimaryDevice(const string id) : ScsiPrimaryCommands(), Device(id)
{
	devices.insert(this);

	ctrl = NULL;

	// Mandatory SCSI primary commands
	AddCommand(eCmdTestUnitReady, "TestUnitReady", &PrimaryDevice::TestUnitReady);
	AddCommand(eCmdInquiry, "Inquiry", &PrimaryDevice::Inquiry);
	AddCommand(eCmdReportLuns, "ReportLuns", &PrimaryDevice::ReportLuns);

	// Optional commands used by all RaSCSI devices
	AddCommand(eCmdRequestSense, "RequestSense", &PrimaryDevice::RequestSense);
}

PrimaryDevice::~PrimaryDevice()
{
	devices.erase(this);
}

void PrimaryDevice::AddCommand(scsi_command opcode, const char* name, void (PrimaryDevice::*execute)(SASIDEV *))
{
	commands[opcode] = new command_t(name, execute);
}

bool PrimaryDevice::Dispatch(SCSIDEV *controller)
{
	ctrl = controller->GetCtrl();

	const auto& it = commands.find(static_cast<scsi_command>(ctrl->cmd[0]));
	if (it != commands.end()) {
		LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, it->second->name, (unsigned int)ctrl->cmd[0]);

		(this->*it->second->execute)(controller);

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

void PrimaryDevice::RequestSense(SASIDEV *controller)
{
	int lun = controller->GetEffectiveLun();

    // Note: According to the SCSI specs the LUN handling for REQUEST SENSE non-existing LUNs do *not* result
	// in CHECK CONDITION. Only the Sense Key and ASC are set in order to signal the non-existing LUN.
	if (!ctrl->unit[lun]) {
        // LUN 0 can be assumed to be present (required to call RequestSense() below)
		lun = 0;

		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_LUN);
		ctrl->status = 0x00;
	}

    ctrl->length = ((PrimaryDevice *)ctrl->unit[lun])->RequestSense(ctrl->cmd, ctrl->buffer);
	ASSERT(ctrl->length > 0);

    LOGTRACE("%s Status $%02X, Sense Key $%02X, ASC $%02X",__PRETTY_FUNCTION__, ctrl->status, ctrl->buffer[2], ctrl->buffer[12]);

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

int PrimaryDevice::RequestSense(const DWORD *cdb, BYTE *buf)
{
	ASSERT(cdb);
	ASSERT(buf);

	// Return not ready only if there are no errors
	if (GetStatusCode() == STATUS_NOERROR) {
		if (!IsReady()) {
			SetStatusCode(STATUS_NOTREADY);
		}
	}

	// Size determination (according to allocation length)
	int size = (int)cdb[4];
	ASSERT((size >= 0) && (size < 0x100));

	// For SCSI-1, transfer 4 bytes when the size is 0
    // (Deleted this specification for SCSI-2)
	if (size == 0) {
		size = 4;
	}

	// Clear the buffer
	memset(buf, 0, size);

	// Set 18 bytes including extended sense data

	// Current error
	buf[0] = 0x70;

	buf[2] = (BYTE)(GetStatusCode() >> 16);
	buf[7] = 10;
	buf[12] = (BYTE)(GetStatusCode() >> 8);
	buf[13] = (BYTE)GetStatusCode();

	return size;
}

