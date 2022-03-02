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
	// EVPD and page code check
	if ((ctrl->cmd[1] & 0x01) || ctrl->cmd[2]) {
		controller->Error(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
		return;
	}

	vector<BYTE> buf = Inquiry();

	size_t allocation_length = ctrl->cmd[4] + (ctrl->cmd[3] << 8);
	if (allocation_length > buf.size()) {
		allocation_length = buf.size();
	}

	memcpy(ctrl->buffer, buf.data(), allocation_length);
	ctrl->length = allocation_length;

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
	int allocation_length = (ctrl->cmd[6] << 24) + (ctrl->cmd[7] << 16) + (ctrl->cmd[8] << 8) + ctrl->cmd[9];

	BYTE *buf = ctrl->buffer;
	memset(buf, 0, allocation_length);

	int size = 0;
	for (int lun = 0; lun < controller->GetCtrl()->device->GetSupportedLuns(); lun++) {
		if (controller->GetCtrl()->unit[lun]) {
			size += 8;
			buf[size + 7] = lun;
		}
	}

	buf[2] = size >> 8;
	buf[3] = size;

	size += 8;

	ctrl->length = allocation_length < size ? allocation_length : size;

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

		controller->Error(sense_key::ILLEGAL_REQUEST, asc::INVALID_LUN);
		ctrl->status = 0x00;
	}

	size_t allocation_length = ctrl->cmd[4];

    vector<BYTE> buf = ((PrimaryDevice *)ctrl->unit[lun])->RequestSense(allocation_length);

    if (allocation_length > buf.size()) {
    	allocation_length = buf.size();
    }

    memcpy(ctrl->buffer, buf.data(), allocation_length);
    ctrl->length = allocation_length;

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

vector<BYTE> PrimaryDevice::Inquiry(device_type type, scsi_level level, bool is_removable) const
{
	vector<BYTE> buf = vector<BYTE>(0x1F + 5);

	// Basic data
	// buf[0] ... SCSI device type
	// buf[1] ... Bit 7: Removable/not removable
	// buf[2] ... SCSI compliance level of command system
	// buf[3] ... SCSI compliance level of Inquiry response
	// buf[4] ... Inquiry additional data
	buf[0] = type;
	buf[1] = is_removable ? 0x80 : 0x00;
	buf[2] = level;
	buf[3] = level >= scsi_level::SCSI_2 ? scsi_level::SCSI_2 : scsi_level::SCSI_1_CCS;
	buf[4] = 0x1F;

	// Padded vendor, product, revision
	memcpy(&buf[8], GetPaddedName().c_str(), 28);

	return buf;
}

vector<BYTE> PrimaryDevice::RequestSense(int)
{
	// Return not ready only if there are no errors
	if (GetStatusCode() == STATUS_NOERROR && !IsReady()) {
		SetStatusCode(STATUS_NOTREADY);
	}

	// Set 18 bytes including extended sense data

	vector<BYTE> buf(18);

	// Current error
	buf[0] = 0x70;

	buf[2] = GetStatusCode() >> 16;
	buf[7] = 10;
	buf[12] = GetStatusCode() >> 8;
	buf[13] = GetStatusCode();

	return buf;
}

bool PrimaryDevice::WriteBytes(BYTE *buf, uint32_t length)
{
	LOGERROR("%s Writing bytes is not supported by this device", __PRETTY_FUNCTION__);

	return false;
}
