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

// Also used by the PiscsiExecutor tests
bool enable_logging; //NOSONAR Must be global in order to be shared with the tests

class Environment final : public ::testing::Environment
{
public:

	Environment() = default;
	~Environment() override = default;

	void SetUp() override { spdlog::set_level(enable_logging ? spdlog::level::trace : spdlog::level::off); }
};

int main(int argc, char *[])
{
	// If any argument is provided the log level is set to trace
	enable_logging = argc > 1;

	testing::AddGlobalTestEnvironment(new Environment());

	testing::InitGoogleTest();

	return RUN_ALL_TESTS();
}
