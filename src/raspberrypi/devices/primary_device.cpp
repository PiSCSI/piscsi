//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "primary_device.h"

using namespace std;

PrimaryDevice::PrimaryDevice(const string id) : Device(id), ScsiPrimaryCommands()
{
	ctrl = NULL;
}
