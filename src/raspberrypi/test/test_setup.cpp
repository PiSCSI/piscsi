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

	Environment() {}
	~Environment() {}

	// Turn off logging
	void SetUp() override { spdlog::set_level(spdlog::level::off); }
};

DeviceFactory& device_factory = DeviceFactory::instance();

ControllerManager& controller_manager = ControllerManager::instance();

int main(int, char*[])
{
  testing::AddGlobalTestEnvironment(new Environment());

  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}
