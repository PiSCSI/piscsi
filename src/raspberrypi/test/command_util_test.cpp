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
