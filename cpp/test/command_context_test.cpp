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
#include "piscsi/command_context.h"
#include <unistd.h>

TEST(CommandContext, ReadCommand)
{
	auto [fd1, filename1] = OpenTempFile();

	CommandContext context1(fd1);

	context1.ReadCommand();

	// Invalid magic
	ASSERT_EQ(3, write(fd1, "abc", 3));
	close(fd1);

	EXPECT_THROW(context1.ReadCommand(), io_exception);

	auto [fd2, filename2] = OpenTempFile();

	CommandContext context2(fd2);

	// Valid magic but invalid (no) command
	ASSERT_EQ(6, write(fd2, "RASCSI", 6));
	close(fd2);

	EXPECT_THROW(context2.ReadCommand(), io_exception);
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
