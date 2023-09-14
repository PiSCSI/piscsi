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

	// Invalid magic with wrong length
	vector data = { byte{'1'}, byte{'2'}, byte{'3'} };
	fd = open(CreateTempFileWithData(data).string().c_str(), O_RDONLY);
	CommandContext context2(fd);
	EXPECT_THROW(context2.ReadCommand(), io_exception);
	close(fd);

	// Invalid magic with right length
	data = { byte{'1'}, byte{'2'}, byte{'3'}, byte{'4'}, byte{'5'}, byte{'6'} };
	fd = open(CreateTempFileWithData(data).string().c_str(), O_RDONLY);
	CommandContext context3(fd);
	EXPECT_THROW(context3.ReadCommand(), io_exception);
	close(fd);

	data = { byte{'R'}, byte{'A'}, byte{'S'}, byte{'C'}, byte{'S'}, byte{'I'}, byte{'1'} };
	// Valid magic but invalid command
	fd = open(CreateTempFileWithData(data).string().c_str(), O_RDONLY);
	CommandContext context4(fd);
	EXPECT_THROW(context4.ReadCommand(), io_exception);
	close(fd);

	data = { byte{'R'}, byte{'A'}, byte{'S'}, byte{'C'}, byte{'S'}, byte{'I'} };
	// Valid magic but missing command
	fd = open(CreateTempFileWithData(data).string().c_str(), O_RDONLY);
	CommandContext context5(fd);
	EXPECT_THROW(context5.ReadCommand(), io_exception);
	close(fd);

	const string filename = CreateTempFileWithData(data).string();
	fd = open(filename.c_str(), O_RDWR | O_APPEND);
	ProtobufSerializer serializer;
	PbCommand command;
	command.set_operation(PbOperation::SERVER_INFO);
	serializer.SerializeMessage(fd, command);
	close(fd);
	fd = open(filename.c_str(), O_RDONLY);
	CommandContext context6(fd);
	context6.ReadCommand();
	close(fd);
	EXPECT_EQ(PbOperation::SERVER_INFO, context6.GetCommand().operation());
}

TEST(CommandContext, GetCommand)
{
	PbCommand command;
	command.set_operation(PbOperation::SERVER_INFO);
	CommandContext context(command);
	EXPECT_EQ(PbOperation::SERVER_INFO, context.GetCommand().operation());
}

TEST(CommandContext, WriteResult)
{
	const string filename = CreateTempFile(0);
	int fd = open(filename.c_str(), O_RDWR | O_APPEND);
	PbResult result;
	result.set_status(false);
	result.set_error_code(PbErrorCode::UNAUTHORIZED);
	CommandContext context(fd);
	context.WriteResult(result);
	close(fd);

	ProtobufSerializer serializer;
	fd = open(filename.c_str(), O_RDONLY);
	result.set_status(true);
	serializer.DeserializeMessage(fd, result);
	close(fd);
	EXPECT_FALSE(result.status());
	EXPECT_EQ(PbErrorCode::UNAUTHORIZED, result.error_code());
}

TEST(CommandContext, IsValid)
{
	PbCommand command;
	CommandContext context1(command);
	EXPECT_FALSE(context1.IsValid());

	const int fd = open(CreateTempFile(0).string().c_str(), O_RDONLY);
	CommandContext context2(fd);
	EXPECT_TRUE(context2.IsValid());
	close(fd);
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

	CommandContext context1(command);
	EXPECT_TRUE(context1.ReturnSuccessStatus());

	const int fd = open("/dev/null", O_RDWR);
	CommandContext context2(fd);
	EXPECT_TRUE(context2.ReturnSuccessStatus());
	close(fd);
}

TEST(CommandContext, ReturnErrorStatus)
{
	PbCommand command;

	CommandContext context1(command);
	EXPECT_FALSE(context1.ReturnErrorStatus("error"));

	const int fd = open("/dev/null", O_RDWR);
	CommandContext context2(fd);
	EXPECT_FALSE(context2.ReturnErrorStatus("error"));
	close(fd);
}
