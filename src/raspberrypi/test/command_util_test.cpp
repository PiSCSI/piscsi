//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "rascsi_interface.pb.h"
#include "command_util.h"

using namespace rascsi_interface;
using namespace command_util;

void TestSpecialDevice(const string& name)
{
	PbDeviceDefinition device;
	ParseParameters(device, name);
	ASSERT_EQ(name, GetParam(device, "file"));
	ASSERT_EQ("", GetParam(device, "interfaces"));
}

TEST(CommandUtil, AddGetParam)
{
	PbCommand command;
	AddParam(command, "key", "value");
	ASSERT_EQ("value", GetParam(command, "key"));
	ASSERT_EQ("", GetParam(command, "xyz"));

	PbDeviceDefinition device;
	AddParam(device, "key", "value");
	ASSERT_EQ("value", GetParam(device, "key"));
	ASSERT_EQ("", GetParam(device, "xyz"));
}

TEST(CommandUtil, ParseParameters)
{
	PbDeviceDefinition device1;
	ParseParameters(device1, "a=b:c=d:e");
	ASSERT_EQ("b", GetParam(device1, "a"));
	ASSERT_EQ("d", GetParam(device1, "c"));
	ASSERT_EQ("", GetParam(device1, "e"));

	// Old style parameters
	PbDeviceDefinition device2;
	ParseParameters(device2, "a");
	ASSERT_EQ("a", GetParam(device2, "file"));
	ASSERT_EQ("a", GetParam(device2, "interfaces"));

	TestSpecialDevice("bridge");
	TestSpecialDevice("daynaport");
	TestSpecialDevice("printer");
	TestSpecialDevice("services");
}
