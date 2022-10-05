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

TEST(CommandContext, ReturnLocalizedError)
{
	CommandContext context("", -1);

	EXPECT_FALSE(context.ReturnLocalizedError(LocalizationKey::ERROR_LOG_LEVEL));
}
