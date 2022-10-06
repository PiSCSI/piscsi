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

TEST(RasctlDisplayTest, DisplayDevicesInfo)
{
	RasctlDisplay display;
	PbDevicesInfo info;

	EXPECT_FALSE(display.DisplayDevicesInfo(info).empty());
}

TEST(RasctlDisplayTest, DisplayDeviceInfo)
{
	RasctlDisplay display;
	PbDevice device;

	EXPECT_FALSE(display.DisplayDeviceInfo(device).empty());
}

TEST(RasctlDisplayTest, DisplayVersionInfo)
{
	RasctlDisplay display;
	PbVersionInfo info;

	EXPECT_FALSE(display.DisplayVersionInfo(info).empty());
}

TEST(RasctlDisplayTest, DisplayLogLevelInfo)
{
	RasctlDisplay display;
	PbLogLevelInfo info;

	EXPECT_FALSE(display.DisplayLogLevelInfo(info).empty());
}

TEST(RasctlDisplayTest, DisplayDeviceTypesInfo)
{
	RasctlDisplay display;
	PbDeviceTypesInfo info;

	// Start with 2 instead of 1. 1 was the removed SASI drive type.
	int ordinal = 2;
	while (PbDeviceType_IsValid(ordinal)) {
		PbDeviceType type = UNDEFINED;
		PbDeviceType_Parse(PbDeviceType_Name((PbDeviceType)ordinal), &type);
		auto type_properties = info.add_properties();
		type_properties->set_type(type);
		ordinal++;
	}

	EXPECT_FALSE(display.DisplayDeviceTypesInfo(info).empty());
}

TEST(RasctlDisplayTest, DisplayReservedIdsInfo)
{
	RasctlDisplay display;
	PbReservedIdsInfo info;

	EXPECT_FALSE(display.DisplayReservedIdsInfo(info).empty());
}

TEST(RasctlDisplayTest, DisplayNetworkInterfacesInfo)
{
	RasctlDisplay display;
	PbNetworkInterfacesInfo info;

	EXPECT_FALSE(display.DisplayNetworkInterfaces(info).empty());
}

TEST(RasctlDisplayTest, DisplayImageFile)
{
	RasctlDisplay display;
	PbImageFile file;

	EXPECT_FALSE(display.DisplayImageFile(file).empty());
}

TEST(RasctlDisplayTest, DisplayImageFilesInfo)
{
	RasctlDisplay display;
	PbImageFilesInfo info;

	EXPECT_FALSE(display.DisplayImageFilesInfo(info).empty());
}

TEST(RasctlDisplayTest, DisplayMappingInfo)
{
	RasctlDisplay display;
	PbMappingInfo info;

	EXPECT_FALSE(display.DisplayMappingInfo(info).empty());
}

TEST(RasctlDisplayTest, DisplayOperationInfo)
{
	RasctlDisplay display;
	PbOperationInfo info;

	EXPECT_FALSE(display.DisplayOperationInfo(info).empty());
}
