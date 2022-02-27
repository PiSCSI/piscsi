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
#include "dispatcher.h"
#include "primary_device.h"

using namespace std;
using namespace scsi_defs;

PrimaryDevice::PrimaryDevice(const string& id) : ScsiPrimaryCommands(), Device(id)
{
	ctrl = NULL;

	// Mandatory SCSI primary commands
	dispatcher.AddCommand(eCmdTestUnitReady, "TestUnitReady", &PrimaryDevice::TestUnitReady);
	dispatcher.AddCommand(eCmdInquiry, "Inquiry", &PrimaryDevice::Inquiry);
	dispatcher.AddCommand(eCmdReportLuns, "ReportLuns", &PrimaryDevice::ReportLuns);

	// Optional commands used by all RaSCSI devices
	dispatcher.AddCommand(eCmdRequestSense, "RequestSense", &PrimaryDevice::RequestSense);
}

bool PrimaryDevice::Dispatch(SCSIDEV *controller)
{
	return dispatcher.Dispatch(this, controller);
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
	ctrl->length = Inquiry(ctrl->cmd, ctrl->buffer);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	int lun = controller->GetEffectiveLun();

	// Report if the device does not support the requested LUN
	if (!ctrl->unit[lun]) {
		LOGTRACE("Reporting LUN %d for device ID %d as not supported", lun, ctrl->device->GetId());

		// Signal that the requested LUN does not exist
		ctrl->buffer[0] |= 0x7f;
	}

	controller->DataIn();
}

void PrimaryDevice::ReportLuns(SASIDEV *controller)
{
	BYTE *buf = ctrl->buffer;

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
	assert(ctrl->length > 0);

    LOGTRACE("%s Status $%02X, Sense Key $%02X, ASC $%02X",__PRETTY_FUNCTION__, ctrl->status, ctrl->buffer[2], ctrl->buffer[12]);

    controller->DataIn();
}

bool PrimaryDevice::CheckReady()
{
	// Not ready if reset
	if (IsReset()) {
		SetStatusCode(STATUS_DEVRESET);
		SetReset(false);
		LOGTRACE("%s Device in reset", __PRETTY_FUNCTION__);
		return false;
	}

	// Not ready if it needs attention
	if (IsAttn()) {
		SetStatusCode(STATUS_ATTENTION);
		SetAttn(false);
		LOGTRACE("%s Device in needs attention", __PRETTY_FUNCTION__);
		return false;
	}

	// Return status if not ready
	if (!IsReady()) {
		SetStatusCode(STATUS_NOTREADY);
		LOGTRACE("%s Device not ready", __PRETTY_FUNCTION__);
		return false;
	}

	// Initialization with no error
	LOGTRACE("%s Device is ready", __PRETTY_FUNCTION__);

	return true;
}

int PrimaryDevice::Inquiry(int type, int scsi_level, bool is_removable, const DWORD *cdb, BYTE *buf)
{
	// EVPD and page code check
	if ((cdb[1] & 0x01) || cdb[2]) {
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}

	int allocation_length = cdb[4] + (cdb[3] << 8);
	if (allocation_length > 4) {
		if (allocation_length > 44) {
			allocation_length = 44;
		}

		// Basic data
		// buf[0] ... SCSI Device type
		// buf[1] ... Bit 7: Removable/not removable
		// buf[2] ... SCSI-2 compliant command system
		// buf[3] ... SCSI-2 compliant Inquiry response
		// buf[4] ... Inquiry additional data
		memset(buf, 0, allocation_length);
		buf[0] = type;
		buf[1] = is_removable ? 0x80 : 0x00;
		buf[2] = scsi_level;
		// Response data format is SCSI-2 for devices supporting SCSI-2 or newer, otherwise it is SCSI-1-CCS
		buf[3] = scsi_level >= 2 ? 2 : 0;
		buf[4] = 0x1F;

		// Padded vendor, product, revision
		memcpy(&buf[8], GetPaddedName().c_str(), 28);
	}

	return allocation_length;
}

int PrimaryDevice::RequestSense(const DWORD *cdb, BYTE *buf)
{
	// Return not ready only if there are no errors
	if (GetStatusCode() == STATUS_NOERROR && !IsReady()) {
		SetStatusCode(STATUS_NOTREADY);
	}

	// Size determination (according to allocation length)
	int size = (int)cdb[4];
	assert(size >= 0 && size < 0x100);

	// For SCSI-1, transfer 4 bytes when the size is 0
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

bool PrimaryDevice::WriteBytes(BYTE *buf, uint32_t length)
{
	LOGERROR("%s Writing bytes is not supported by this device", __PRETTY_FUNCTION__);

	return false;
}
