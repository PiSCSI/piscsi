//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// A base device with mode page support
//
//---------------------------------------------------------------------------

#include "mode_page_device.h"

using namespace std;

ModePageDevice::ModePageDevice(const string id) : PrimaryDevice(id)
{
}
