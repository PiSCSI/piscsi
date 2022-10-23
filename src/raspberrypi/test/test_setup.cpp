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

class Environment final : public ::testing::Environment
{
	spdlog::level::level_enum log_level;

public:

	explicit Environment(spdlog::level::level_enum level) : log_level(level) {}
	~Environment() override = default;

	void SetUp() override { spdlog::set_level(log_level); }
};

int main(int argc, char *[])
{
	// If any argument is provided the log level is set to trace
	testing::AddGlobalTestEnvironment(new Environment(argc > 1 ? spdlog::level::trace : spdlog::level::off));

	testing::InitGoogleTest();

	return RUN_ALL_TESTS();
}
