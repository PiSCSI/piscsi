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
