//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "rascsi_exceptions.h"
#include "dispatcher.h"
#include "primary_device.h"

using namespace std;
using namespace scsi_defs;

PrimaryDevice::PrimaryDevice(const string& id) : Device(id)
{
	// Mandatory SCSI primary commands
	dispatcher.Add(scsi_command::eCmdTestUnitReady, "TestUnitReady", &PrimaryDevice::TestUnitReady);
	dispatcher.Add(scsi_command::eCmdInquiry, "Inquiry", &PrimaryDevice::Inquiry);
	dispatcher.Add(scsi_command::eCmdReportLuns, "ReportLuns", &PrimaryDevice::ReportLuns);

	// Optional commands used by all RaSCSI devices
	dispatcher.Add(scsi_command::eCmdRequestSense, "RequestSense", &PrimaryDevice::RequestSense);
}

bool PrimaryDevice::Dispatch(scsi_command cmd)
{
	return dispatcher.Dispatch(this, cmd);
}

void PrimaryDevice::SetController(AbstractController *c)
{
	controller = c;
	ctrl = controller->GetCtrl();
}

void PrimaryDevice::TestUnitReady()
{
	CheckReady();

	EnterStatusPhase();
}

void PrimaryDevice::Inquiry()
{
	// EVPD and page code check
	if ((ctrl->cmd[1] & 0x01) || ctrl->cmd[2]) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	vector<byte> buf = InquiryInternal();

	size_t allocation_length = ctrl->cmd[4] + (ctrl->cmd[3] << 8);
	if (allocation_length > buf.size()) {
		allocation_length = buf.size();
	}

	memcpy(ctrl->buffer, buf.data(), allocation_length);
	ctrl->length = (uint32_t)allocation_length;

	// Report if the device does not support the requested LUN
	if (int lun = controller->GetEffectiveLun(); !controller->HasDeviceForLun(lun)) {
		LOGTRACE("Reporting LUN %d for device ID %d as not supported", lun, GetId())

		// Signal that the requested LUN does not exist
		ctrl->buffer[0] |= 0x7f;
	}

	EnterDataInPhase();
}

void PrimaryDevice::ReportLuns()
{
	int allocation_length = (ctrl->cmd[6] << 24) + (ctrl->cmd[7] << 16) + (ctrl->cmd[8] << 8) + ctrl->cmd[9];

	BYTE *buf = ctrl->buffer;
	memset(buf, 0, allocation_length < ctrl->bufsize ? allocation_length : ctrl->bufsize);

	int size = 0;

	// Only SELECT REPORT mode 0 is supported
	if (ctrl->cmd[2]) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	for (int lun = 0; lun < controller->GetMaxLuns(); lun++) {
		if (controller->HasDeviceForLun(lun)) {
			size += 8;
			buf[size + 7] = (BYTE)lun;
		}
	}

	buf[2] = (BYTE)(size >> 8);
	buf[3] = (BYTE)size;

	size += 8;

	ctrl->length = allocation_length < size ? allocation_length : size;

	EnterDataInPhase();
}

void PrimaryDevice::RequestSense()
{
	int lun = controller->GetEffectiveLun();

    // Note: According to the SCSI specs the LUN handling for REQUEST SENSE non-existing LUNs do *not* result
	// in CHECK CONDITION. Only the Sense Key and ASC are set in order to signal the non-existing LUN.
	if (!controller->HasDeviceForLun(lun)) {
        // LUN 0 can be assumed to be present (required to call RequestSense() below)
		assert(controller->HasDeviceForLun(0));

		lun = 0;

		// Do not raise an exception here because the rest of the code must be executed
		controller->Error(sense_key::ILLEGAL_REQUEST, asc::INVALID_LUN);

		ctrl->status = 0x00;
	}

    vector<byte> buf = controller->GetDeviceForLun(lun)->HandleRequestSense();

	size_t allocation_length = ctrl->cmd[4];
    if (allocation_length > buf.size()) {
    	allocation_length = buf.size();
    }

    memcpy(ctrl->buffer, buf.data(), allocation_length);
    ctrl->length = (uint32_t)allocation_length;

    EnterDataInPhase();
}

void PrimaryDevice::CheckReady()
{
	// Not ready if reset
	if (IsReset()) {
		SetReset(false);
		LOGTRACE("%s Device in reset", __PRETTY_FUNCTION__)
		throw scsi_error_exception(sense_key::UNIT_ATTENTION, asc::POWER_ON_OR_RESET);
	}

	// Not ready if it needs attention
	if (IsAttn()) {
		SetAttn(false);
		LOGTRACE("%s Device in needs attention", __PRETTY_FUNCTION__)
		throw scsi_error_exception(sense_key::UNIT_ATTENTION, asc::NOT_READY_TO_READY_CHANGE);
	}

	// Return status if not ready
	if (!IsReady()) {
		LOGTRACE("%s Device not ready", __PRETTY_FUNCTION__)
		throw scsi_error_exception(sense_key::NOT_READY, asc::MEDIUM_NOT_PRESENT);
	}

	// Initialization with no error
	LOGTRACE("%s Device is ready", __PRETTY_FUNCTION__)
}

vector<byte> PrimaryDevice::HandleInquiry(device_type type, scsi_level level, bool is_removable) const
{
	vector<byte> buf(0x1F + 5);

	// Basic data
	// buf[0] ... SCSI device type
	// buf[1] ... Bit 7: Removable/not removable
	// buf[2] ... SCSI compliance level of command system
	// buf[3] ... SCSI compliance level of Inquiry response
	// buf[4] ... Inquiry additional data
	buf[0] = (byte)type;
	buf[1] = (byte)(is_removable ? 0x80 : 0x00);
	buf[2] = (byte)level;
	buf[3] = (byte)(level >= scsi_level::SCSI_2 ? scsi_level::SCSI_2 : scsi_level::SCSI_1_CCS);
	buf[4] = (byte)0x1F;

	// Padded vendor, product, revision
	memcpy(&buf[8], GetPaddedName().c_str(), 28);

	return buf;
}

vector<byte> PrimaryDevice::HandleRequestSense() const
{
	// Return not ready only if there are no errors
	if (!GetStatusCode() && !IsReady()) {
		throw scsi_error_exception(sense_key::NOT_READY, asc::MEDIUM_NOT_PRESENT);
	}

	// Set 18 bytes including extended sense data

	vector<byte> buf(18);

	// Current error
	buf[0] = (byte)0x70;

	buf[2] = (byte)(GetStatusCode() >> 16);
	buf[7] = (byte)10;
	buf[12] = (byte)(GetStatusCode() >> 8);
	buf[13] = (byte)GetStatusCode();

	LOGTRACE("%s Status $%02X, Sense Key $%02X, ASC $%02X",__PRETTY_FUNCTION__, ctrl->status, ctrl->buffer[2], ctrl->buffer[12])

	return buf;
}

bool PrimaryDevice::WriteByteSequence(BYTE *, uint32_t)
{
	LOGERROR("%s Writing bytes is not supported by this device", __PRETTY_FUNCTION__)

	return false;
}
