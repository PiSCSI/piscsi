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
	const bool disable_logging = argc <= 1;

	// If any argument is provided the log level is set to trace
	spdlog::set_level(disable_logging ? spdlog::level::off : spdlog::level::trace);

	int fd = -1;

	if (disable_logging) {
		fd = open("/dev/null", O_WRONLY);
		dup2(fd, STDERR_FILENO);
	}

	testing::InitGoogleTest();

	const int result = RUN_ALL_TESTS();

	if (fd != -1) {
		close(fd);
	}

	return result;
}
