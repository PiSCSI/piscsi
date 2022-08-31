//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "mocks.h"
#include "controllers/controller_manager.h"

TEST(ControllerManagerTest, CreateScsiController)
{
	controller_manager.CreateScsiController(nullptr, new MockPrimaryDevice());
}
