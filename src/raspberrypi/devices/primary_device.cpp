//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "primary_device.h"

using namespace std;

PrimaryDevice::PrimaryDevice(const string id) : Device(id), ScsiPrimaryCommands()
{
	ctrl = NULL;
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
