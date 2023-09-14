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
