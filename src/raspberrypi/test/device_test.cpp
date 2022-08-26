//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Unit tests based on GoogleTest and GoogleMock
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "../devices/device.h"
#include "../devices/device_factory.h"

using namespace rascsi_interface;

namespace DeviceTest
{

DeviceFactory& device_factory = DeviceFactory::instance();

TEST(DeviceTest, SCHD_Device)
{
	Device *device;

	device = device_factory.CreateDevice(UNDEFINED, "test");
	EXPECT_EQ(nullptr, device);

	device = device_factory.CreateDevice(SCHD, "test");
	EXPECT_NE(nullptr, device);
	EXPECT_EQ(true, device->IsProtectable());
	EXPECT_EQ(false, device->IsProtected());
	EXPECT_EQ(false, device->IsReadOnly());
	EXPECT_EQ(false, device->IsRemovable());
	EXPECT_EQ(false, device->IsRemoved());
	EXPECT_EQ(false, device->IsLockable());
	EXPECT_EQ(false, device->IsLocked());
	EXPECT_EQ(true, device->IsStoppable());
	EXPECT_EQ(false, device->IsStopped());
}

}
