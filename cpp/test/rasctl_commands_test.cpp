//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// These tests only test up the point where a network connection is required.
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "shared/protobuf_util.h"
#include "shared/rascsi_exceptions.h"
#include "generated/rascsi_interface.pb.h"
#include "rasctl/rasctl_commands.h"

using namespace testing;
using namespace rascsi_interface;
using namespace protobuf_util;

TEST(RasctlCommandsTest, Execute)
{
	PbCommand command;
	RasctlCommands commands(command, "localhost", 0);

	command.set_operation(LOG_LEVEL);
	EXPECT_THROW(commands.Execute("log_level", "", "", "", ""), io_exception);
	EXPECT_EQ("log_level", GetParam(command, "level"));

	command.set_operation(DEFAULT_FOLDER);
	EXPECT_THROW(commands.Execute("", "default_folder", "", "", ""), io_exception);
	EXPECT_EQ("default_folder", GetParam(command, "folder"));

	command.set_operation(RESERVE_IDS);
	EXPECT_THROW(commands.Execute("", "", "reserved_ids", "", ""), io_exception);
	EXPECT_EQ("reserved_ids", GetParam(command, "ids"));

	command.set_operation(CREATE_IMAGE);
	EXPECT_FALSE(commands.Execute("", "", "", "", ""));
	EXPECT_THROW(commands.Execute("", "", "", "filename:0", ""), io_exception);
	EXPECT_EQ("false", GetParam(command, "read_only"));

	command.set_operation(DELETE_IMAGE);
	EXPECT_THROW(commands.Execute("", "", "", "filename1", ""), io_exception);
	EXPECT_EQ("filename1", GetParam(command, "file"));

	command.set_operation(RENAME_IMAGE);
	EXPECT_FALSE(commands.Execute("", "", "", "", ""));
	EXPECT_THROW(commands.Execute("", "", "", "from1:to1", ""), io_exception);
	EXPECT_EQ("from1", GetParam(command, "from"));
	EXPECT_EQ("to1", GetParam(command, "to"));

	command.set_operation(COPY_IMAGE);
	EXPECT_FALSE(commands.Execute("", "", "", "", ""));
	EXPECT_THROW(commands.Execute("", "", "", "from2:to2", ""), io_exception);
	EXPECT_EQ("from2", GetParam(command, "from"));
	EXPECT_EQ("to2", GetParam(command, "to"));

	command.set_operation(DEVICES_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

	command.set_operation(DEVICE_TYPES_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

	command.set_operation(VERSION_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

	command.set_operation(SERVER_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

	command.set_operation(DEFAULT_IMAGE_FILES_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

	command.set_operation(IMAGE_FILE_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", "filename2"), io_exception);
	EXPECT_EQ("filename2", GetParam(command, "file"));

	command.set_operation(NETWORK_INTERFACES_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

	command.set_operation(LOG_LEVEL_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

	command.set_operation(RESERVED_IDS_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

	command.set_operation(MAPPING_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

	command.set_operation(OPERATION_INFO);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);

	command.set_operation(NO_OPERATION);
	EXPECT_THROW(commands.Execute("", "", "", "", ""), io_exception);
}

TEST(RasctlCommandsTest, CommandDevicesInfo)
{
	PbCommand command;

	RasctlCommands commands1(command, "/invalid_host_name", 0);
	EXPECT_THROW(commands1.CommandDevicesInfo(), io_exception);

	RasctlCommands commands2(command, "localhost", 0);
	EXPECT_THROW(commands2.CommandDevicesInfo(), io_exception);
}
