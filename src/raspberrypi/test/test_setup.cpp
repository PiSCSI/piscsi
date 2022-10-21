//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "spdlog/spdlog.h"

bool enable_logging;

int main(int argc, char *[])
{
	// If any argument is provided log on trace level
	enable_logging = argc > 1;

	testing::InitGoogleTest();

	return RUN_ALL_TESTS();
}

void testing::Test::SetUp()
{
	// Re-configure logging before each test, because some tests change the log level
	spdlog::set_level(enable_logging ? spdlog::level::trace : spdlog::level::off);
}
