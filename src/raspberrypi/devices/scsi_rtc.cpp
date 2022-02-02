//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsi_rtc.h"

SCSIRtc::SCSIRtc() : Disk("SCRT")
{
	ctrl = 0;
}

SCSIRtc::~SCSIRtc()
{
}

bool SCSIRtc::Dispatch(SCSIDEV *controller)
{
	ctrl = controller->GetCtrl();

	// Only certain commands are supported
	if (ctrl->cmd[0] == SCSIDEV::eCmdTestUnitReady || ctrl->cmd[0] == SCSIDEV::eCmdRequestSense
			|| ctrl->cmd[0] == SCSIDEV::eCmdInquiry || ctrl->cmd[0] == SCSIDEV::eCmdReportLuns
			|| ctrl->cmd[0] == SCSIDEV::eCmdModeSense6 || ctrl->cmd[0] == SCSIDEV::eCmdModeSense10) {
		LOGTRACE("%s Calling base class for dispatching $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->cmd[0]);

		return Disk::Dispatch(controller);
	}

	return false;
}

void SCSIRtc::TestUnitReady(SASIDEV *controller)
{
	// Always successful
	controller->Status();
}

int SCSIRtc::Inquiry(const DWORD *cdb, BYTE *buf)
{
	int allocation_length = cdb[4] + (((DWORD)cdb[3]) << 8);
	if (allocation_length > 4) {
		if (allocation_length > 44) {
			allocation_length = 44;
		}

		// Basic data
		// buf[0] ... Processor Device
		// buf[1] ... Not removable
		// buf[2] ... SCSI-2 compliant command system
		// buf[3] ... SCSI-2 compliant Inquiry response
		// buf[4] ... Inquiry additional data
		memset(buf, 0, allocation_length);
		buf[0] = 0x03;
		buf[2] = 0x01;
		buf[4] = 0x1F;

		// Padded vendor, product, revision
		memcpy(&buf[8], GetPaddedName().c_str(), 28);
	}

	return allocation_length;
}

int SCSIRtc::AddVendorPage(int page, bool, BYTE *buf)
{
	if (page == 0x20) {
		time_t t = time(NULL);
		buf[0] = t >> 24;
		buf[1] = t >> 16;
		buf[2] = t >> 8;
		buf[3] = t;

		return 4;
	}

	return 0;
}

