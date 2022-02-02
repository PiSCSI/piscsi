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
	AddCommand(SCSIDEV::eCmdTestUnitReady, "TestUnitReady", &Disk::TestUnitReady);
	AddCommand(SCSIDEV::eCmdRequestSense, "RequestSense", &Disk::RequestSense);
	AddCommand(SCSIDEV::eCmdInquiry, "Inquiry", &Disk::Inquiry);
	AddCommand(SCSIDEV::eCmdReportLuns, "ReportLuns", &Disk::ReportLuns);

	AddCommand(SCSIDEV::eCmdModeSense6, "ModeSense6", &SCSIRtc::ModeSense6);
	AddCommand(SCSIDEV::eCmdModeSense10, "ModeSense10", &SCSIRtc::ModeSense10);
}

SCSIRtc::~SCSIRtc()
{
}

void SCSIRtc::AddCommand(SCSIDEV::scsi_command opcode, const char* name, void (SCSIRtc::*execute)(SASIDEV *))
{
	commands[opcode] = new command_t(name, execute);
}

bool SCSIRtc::Dispatch(SCSIDEV *controller)
{
	ctrl = controller->GetCtrl();

	if (commands.count(static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0]))) {
		command_t *command = commands[static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0])];

		LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, command->name, (unsigned int)ctrl->cmd[0]);

		(this->*command->execute)(controller);

		return true;
	}

	return false;
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

void SCSIRtc::ModeSense6(SASIDEV *controller)
{
	ctrl->length = Disk::ModeSense6(ctrl->cmd, ctrl->buffer);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataIn();
}

void SCSIRtc::ModeSense10(SASIDEV *controller)
{
	ctrl->length = Disk::ModeSense10(ctrl->cmd, ctrl->buffer);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataIn();
}
