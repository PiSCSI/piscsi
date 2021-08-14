//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "os.h"
#include "device.h"

// TODO Add more primary commands
class PrimaryDevice : public Device
{
public:

	PrimaryDevice(const string& id) : Device(id) {};
	virtual ~PrimaryDevice() {};

};
