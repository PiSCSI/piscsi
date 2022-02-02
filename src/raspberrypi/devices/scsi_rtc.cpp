//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsi_rtc.h"

bool SCSIRtc::Dispatch(SCSIDEV *controller)
{
	unsigned int cmd = controller->GetCtrl()->cmd[0];

	// Only certain commands are supported
	if (cmd == SCSIDEV::eCmdTestUnitReady || cmd == SCSIDEV::eCmdRequestSense
			|| cmd == SCSIDEV::eCmdInquiry || cmd == SCSIDEV::eCmdReportLuns
			|| cmd == SCSIDEV::eCmdModeSense6 || cmd == SCSIDEV::eCmdModeSense10) {
		LOGTRACE("%s Calling base class for dispatching $%02X", __PRETTY_FUNCTION__, cmd);

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
		// Data structure version 1.0
		buf[0] = 0x01;
		buf[1] = 0x00;

		std::time_t t = std::time(NULL);
		std::tm tm = *std::localtime(&t);
		buf[2] = tm.tm_year;
		buf[3] = tm.tm_mon;
		buf[4] = tm.tm_mday;
		buf[5] = tm.tm_hour;
		buf[6] = tm.tm_min;
		// Ignore leap second for simplicity
		buf[7] = tm.tm_sec < 60 ? tm.tm_sec : 59;

		return 8;
	}

	return 0;
}

