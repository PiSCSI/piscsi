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
#include "controllers/controller_manager.h"
#include "devices/device_factory.h"

class Environment : public ::testing::Environment
{
public:

	Environment() = default;
	~Environment() final = default;

	// Turn off logging
	void SetUp() override { spdlog::set_level(spdlog::level::off); }
};

const DeviceFactory& device_factory = DeviceFactory::instance();

int main(int, char*[])
{
  testing::AddGlobalTestEnvironment(new Environment());

  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}
