//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "primary_device.h"
#include <string>

using namespace std;

class ModePageDevice: public PrimaryDevice
{
public:

	ModePageDevice(const string);
	virtual ~ModePageDevice() {};

	virtual int ModeSense6(const DWORD *, BYTE *) = 0;
	virtual int ModeSense10(const DWORD *, BYTE *) = 0;
};
