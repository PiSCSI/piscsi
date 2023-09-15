//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <spdlog/spdlog.h>

int main(int argc, char *[])
{
	// If any argument is provided the log level is set to trace
	spdlog::set_level(argc > 1 ? spdlog::level::trace : spdlog::level::off);

	testing::InitGoogleTest();

	return RUN_ALL_TESTS();
}
