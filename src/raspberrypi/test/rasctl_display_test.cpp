//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "rasctl/rasctl_display.h"

TEST(RasctlDisplayTest, DisplayDeviceInfo)
{
	RasctlDisplay display;
	PbDevice device;

	EXPECT_FALSE(display.DisplayDeviceInfo(device).empty());
}
