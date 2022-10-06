//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gmock/gmock.h>

#include "rascsi/command_context.h"

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
	CommandContext context("", -1);

	EXPECT_FALSE(context.ReturnLocalizedError(LocalizationKey::ERROR_LOG_LEVEL));
}
