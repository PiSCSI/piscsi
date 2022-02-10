//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// A base device implementing mandatory SCSI primary commands
//
//---------------------------------------------------------------------------

#pragma once

#include "controllers/sasidev_ctrl.h"
#include "interfaces/scsi_primary_commands.h"
#include "device.h"
#include <string>

using namespace std;

class PrimaryDevice: public Device, virtual public ScsiPrimaryCommands
{
public:

	PrimaryDevice(const string);
	virtual ~PrimaryDevice() {}

	virtual void TestUnitReady(SASIDEV *) = 0;
	virtual void Inquiry(SASIDEV *) = 0;
	void ReportLuns(SASIDEV *);

	bool CheckReady();

protected:

	SASIDEV::ctrl_t *ctrl;
};
