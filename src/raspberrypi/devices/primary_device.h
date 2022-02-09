//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "controllers/sasidev_ctrl.h"
#include "interfaces/scsi_primary_commands.h"
#include "device.h"
#include <string>

using namespace std;

class PrimaryDevice: public Device, public ScsiPrimaryCommands
{
public:

	PrimaryDevice(const string);
	virtual ~PrimaryDevice() {};

protected:

	SASIDEV::ctrl_t *ctrl;
};
