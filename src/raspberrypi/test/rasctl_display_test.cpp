//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// These tests only test key aspects of the expected output, because the output may change over time.
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

	device.mutable_properties()->set_supports_file(true);
	auto file = make_unique<PbImageFile>();
	file->set_name("filename");
	device.set_allocated_file(file.release());
	s = display.DisplayDeviceInfo(device);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("filename"));

	device.mutable_properties()->set_supports_params(true);
	(*device.mutable_params())["key1"] = "value1";
	s = display.DisplayDeviceInfo(device);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("key1=value1"));
	(*device.mutable_params())["key2"] = "value2";
	s = display.DisplayDeviceInfo(device);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("key1=value1"));
	EXPECT_NE(string::npos, s.find("key2=value2"));
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
			type_properties->mutable_properties()->set_supports_file(true);
			type_properties->mutable_properties()->add_block_sizes(512);
			type_properties->mutable_properties()->add_block_sizes(1024);
		}

		if (type == SCMO) {
			type_properties->mutable_properties()->set_supports_file(true);
			type_properties->mutable_properties()->set_read_only(true);
			type_properties->mutable_properties()->set_protectable(true);
			type_properties->mutable_properties()->set_stoppable(true);
			type_properties->mutable_properties()->set_removable(true);
			type_properties->mutable_properties()->set_lockable(true);
		}

		if (type == SCLP) {
			type_properties->mutable_properties()->set_supports_params(true);
			(*type_properties->mutable_properties()->mutable_default_params())["key1"] = "value1";
			(*type_properties->mutable_properties()->mutable_default_params())["key2"] = "value2";
		}

		ordinal++;
	}

	const string s = display.DisplayDeviceTypesInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("key1=value1"));
	EXPECT_NE(string::npos, s.find("key2=value2"));
}

TEST(RasctlDisplayTest, DisplayReservedIdsInfo)
{
	RasctlDisplay display;
	PbReservedIdsInfo info;

	string s = display.DisplayReservedIdsInfo(info);
	EXPECT_FALSE(s.empty());

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

	file.set_name("filename");
	s = display.DisplayImageFile(file);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("filename"));
	EXPECT_EQ(string::npos, s.find("read-only"));
	EXPECT_EQ(string::npos, s.find("SCHD"));

	file.set_read_only(true);
	s = display.DisplayImageFile(file);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("filename"));
	EXPECT_NE(string::npos, s.find("read-only"));
	EXPECT_EQ(string::npos, s.find("SCHD"));

	file.set_type(SCHD);
	s = display.DisplayImageFile(file);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("SCHD"));
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

	string s = display.DisplayOperationInfo(info);
	EXPECT_FALSE(s.empty());

	PbOperationMetaData meta_data;
	PbOperationParameter *param1 = meta_data.add_parameters();
	param1->set_name("default_key1");
	param1->set_default_value("default_value1");
	PbOperationParameter *param2 = meta_data.add_parameters();
	param2->set_name("default_key2");
	param2->set_default_value("default_value2");
	param2->set_description("description");
	param2->add_permitted_values("permitted_value");
	(*info.mutable_operations())[0] = meta_data;
	s = display.DisplayOperationInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find(PbOperation_Name(NO_OPERATION)));

	meta_data.set_server_side_name(PbOperation_Name(ATTACH));
	(*info.mutable_operations())[0] = meta_data;
	s = display.DisplayOperationInfo(info);
	EXPECT_FALSE(s.empty());
	EXPECT_NE(string::npos, s.find("default_key1"));
	EXPECT_NE(string::npos, s.find("default_value1"));
	EXPECT_NE(string::npos, s.find("default_key2"));
	EXPECT_NE(string::npos, s.find("default_value2"));
	EXPECT_NE(string::npos, s.find("description"));
	EXPECT_NE(string::npos, s.find("permitted_value"));
}
