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

	PbDeviceDefinition device3;
	ParseParameters(device3, "bridge");
	ASSERT_EQ("bridge", GetParam(device3, "file"));
	ASSERT_EQ("", GetParam(device3, "interfaces"));

	PbDeviceDefinition device4;
	ParseParameters(device4, "daynaport");
	ASSERT_EQ("daynaport", GetParam(device4, "file"));
	ASSERT_EQ("", GetParam(device4, "interfaces"));

	PbDeviceDefinition device5;
	ParseParameters(device5, "printer");
	ASSERT_EQ("printer", GetParam(device5, "file"));
	ASSERT_EQ("", GetParam(device5, "interfaces"));

	PbDeviceDefinition device6;
	ParseParameters(device6, "services");
	ASSERT_EQ("services", GetParam(device6, "file"));
	ASSERT_EQ("", GetParam(device6, "interfaces"));
}
