//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "test/test_shared.h"
#include "shared/piscsi_exceptions.h"
#include "shared/protobuf_serializer.h"
#include "piscsi/command_context.h"
#include <fcntl.h>
#include <unistd.h>

TEST(CommandContext, ReadCommand)
{
	int fd = open(CreateTempFile(0).string().c_str(), O_RDONLY);
	CommandContext context1(fd);
	context1.ReadCommand();
	close(fd);

	// Invalid magic
	vector<int> data = { '1', '2', '3', '4', '5', '6' };
	fd = open(CreateTempFileWithData(data).string().c_str(), O_RDONLY);
	CommandContext context2(fd);
	EXPECT_THROW(context2.ReadCommand(), io_exception);
	close(fd);

	data = { 'R', 'A', 'S', 'C', 'S', 'I' };
	// Valid magic but invalid (no) command
	fd = open(CreateTempFileWithData(data).string().c_str(), O_RDONLY);
	CommandContext context3(fd);
	EXPECT_THROW(context3.ReadCommand(), io_exception);
	close(fd);

	fd = open(CreateTempFileWithData(data).string().c_str(), O_RDWR);
	ProtobufSerializer serializer;
	PbCommand command;
	serializer.SerializeMessage(fd, command);
	CommandContext context4(fd);
	context4.ReadCommand();
	close(fd);
}

TEST(CommandContext, IsValid)
{
	PbCommand command;
	CommandContext context(command);

	EXPECT_FALSE(context.IsValid());
}

TEST(CommandContext, ReturnLocalizedError)
{
	PbCommand command;
	CommandContext context(command, "en_US");

	EXPECT_FALSE(context.ReturnLocalizedError(LocalizationKey::ERROR_LOG_LEVEL));
}

TEST(CommandContext, ReturnSuccessStatus)
{
	PbCommand command;
	CommandContext context(command);

	EXPECT_TRUE(context.ReturnSuccessStatus());
}

TEST(CommandContext, ReturnErrorStatus)
{
	PbCommand command;
	CommandContext context(command);

	EXPECT_FALSE(context.ReturnErrorStatus("error"));
}
