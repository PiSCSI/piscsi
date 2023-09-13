//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "piscsi/command_context.h"

TEST(CommandContext, GetSerializer)
{
	CommandContext context("", -1);

	// There is nothing more that can be tested
	context.GetSerializer();
}

TEST(CommandContext, IsValid)
{
	CommandContext context("", -1);

	EXPECT_FALSE(context.IsValid());

	context.SetFd(1);
	EXPECT_TRUE(context.IsValid());
}

TEST(CommandContext, Cleanup)
{
	CommandContext context("", 0);

	EXPECT_EQ(0, context.GetFd());
	context.Cleanup();
	EXPECT_EQ(-1, context.GetFd());
}

TEST(CommandContext, ReturnLocalizedError)
{
	CommandContext context("en_US", -1);

	EXPECT_FALSE(context.ReturnLocalizedError(LocalizationKey::ERROR_LOG_LEVEL));
}

TEST(CommandContext, ReturnSuccessStatus)
{
	CommandContext context("", -1);

	EXPECT_TRUE(context.ReturnSuccessStatus());
}

TEST(CommandContext, ReturnErrorStatus)
{
	CommandContext context("", -1);

	EXPECT_FALSE(context.ReturnErrorStatus("error"));
}
