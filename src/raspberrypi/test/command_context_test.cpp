//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "rascsi/command_context.h"

using namespace rascsi_interface;

TEST(CommandContext, ReturnLocalizedError)
{
	MockCommandContext context;

	EXPECT_FALSE(context.ReturnLocalizedError(LocalizationKey::ERROR_LOG_LEVEL));
}
