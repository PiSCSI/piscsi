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

	device.mutable_properties()->set_supports_file(true);
	device.mutable_properties()->set_read_only(true);
	device.mutable_properties()->set_protectable(true);
	device.mutable_status()->set_protected_(true);
	device.mutable_properties()->set_stoppable(true);
	device.mutable_status()->set_stopped(true);
	device.mutable_properties()->set_removable(true);
	device.mutable_status()->set_removed(true);
	device.mutable_properties()->set_lockable(true);
	device.mutable_status()->set_locked(true);
	EXPECT_FALSE(display.DisplayDeviceInfo(device).empty());

	device.set_block_size(1234);
	string s = display.DisplayDeviceInfo(device);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("1234"));

	device.set_block_count(4321);
	s = display.DisplayDeviceInfo(device);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find(to_string(1234 *4321)));
}

TEST(RasctlDisplayTest, DisplayVersionInfo)
{
	RasctlDisplay display;
	PbVersionInfo info;

	info.set_major_version(1);
	info.set_minor_version(2);
	info.set_patch_version(3);
	string s = display.DisplayVersionInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_EQ(string::npos, s.find("development version"));
	EXPECT_NE(string::npos, s.find("01.02.3"));

	info.set_patch_version(-1);
	s = display.DisplayVersionInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("development version"));
	EXPECT_NE(string::npos, s.find("01.02"));
}

TEST(RasctlDisplayTest, DisplayLogLevelInfo)
{
	RasctlDisplay display;
	PbLogLevelInfo info;

	string s = display.DisplayLogLevelInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_EQ(string::npos, s.find("test"));

	info.add_log_levels("test");
	s = display.DisplayLogLevelInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("test"));
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

		if (type == SCHD) {
			type_properties->mutable_properties()->add_block_sizes(512);
			type_properties->mutable_properties()->add_block_sizes(1024);
			type_properties->mutable_properties()->set_supports_file(true);
		}
		if (type == SCLP) {
			type_properties->mutable_properties()->set_supports_params(true);
		}

		ordinal++;
	}

	EXPECT_FALSE(display.DisplayDeviceTypesInfo(info).empty());
}

TEST(RasctlDisplayTest, DisplayReservedIdsInfo)
{
	RasctlDisplay display;
	PbReservedIdsInfo info;

	string s = display.DisplayReservedIdsInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_EQ(string::npos, s.find("5"));
	EXPECT_EQ(string::npos, s.find("5, 6"));

	info.mutable_ids()->Add(5);
	s = display.DisplayReservedIdsInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("5"));

	info.mutable_ids()->Add(6);
	s = display.DisplayReservedIdsInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("5, 6"));
}

TEST(RasctlDisplayTest, DisplayNetworkInterfacesInfo)
{
	RasctlDisplay display;
	PbNetworkInterfacesInfo info;

	string s = display.DisplayNetworkInterfaces(info);
	EXPECT_FALSE(s.empty());
	EXPECT_EQ(string::npos, s.find("eth0"));
	EXPECT_EQ(string::npos, s.find("wlan0"));

	info.mutable_name()->Add("eth0");
	s = display.DisplayNetworkInterfaces(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("eth0"));

	info.mutable_name()->Add("wlan0");
	s = display.DisplayNetworkInterfaces(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("eth0, wlan0"));
}

TEST(RasctlDisplayTest, DisplayImageFile)
{
	RasctlDisplay display;
	PbImageFile file;

	string s = display.DisplayImageFile(file);
	EXPECT_FALSE(s.empty());
	EXPECT_EQ(string::npos, s.find("filename"));
	EXPECT_EQ(string::npos, s.find("read-only"));

	file.set_name("filename");
	s = display.DisplayImageFile(file);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("filename"));
	EXPECT_EQ(string::npos, s.find("read-only"));

	file.set_read_only(true);
	s = display.DisplayImageFile(file);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("filename"));
	EXPECT_NE(string::npos, s.find("read-only"));
}

TEST(RasctlDisplayTest, DisplayImageFilesInfo)
{
	RasctlDisplay display;
	PbImageFilesInfo info;

	string s = display.DisplayImageFilesInfo(info);
	EXPECT_FALSE(display.DisplayImageFilesInfo(info).empty());
	EXPECT_EQ(string::npos, s.find("filename"));

	PbImageFile *file = info.add_image_files();
	file->set_name("filename");
	s = display.DisplayImageFilesInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("filename"));
}

TEST(RasctlDisplayTest, DisplayMappingInfo)
{
	RasctlDisplay display;
	PbMappingInfo info;

	string s = display.DisplayMappingInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_EQ(string::npos, s.find("key->SCHD"));

	(*info.mutable_mapping())["key"] = SCHD;
	s = display.DisplayMappingInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("key->SCHD"));
}

TEST(RasctlDisplayTest, DisplayOperationInfo)
{
	RasctlDisplay display;
	PbOperationInfo info;

	EXPECT_FALSE(display.DisplayOperationInfo(info).empty());
}
