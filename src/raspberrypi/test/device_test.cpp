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

#include "rascsi_version.h"
#include "../devices/device.h"
#include "../devices/device_factory.h"

using namespace rascsi_interface;

namespace DeviceTest
{

DeviceFactory& device_factory = DeviceFactory::instance();

TEST(DeviceTest, UnknownDeviceType)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "test");
	EXPECT_EQ(nullptr, device);
}

TEST(DeviceTest, SCHD_Device_Defaults)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "test.hda");
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

	EXPECT_EQ("QUANTUM", device->GetVendor()) << "Invalid default vendor for Apple drive";
	EXPECT_EQ("FIREBALL", device->GetProduct()) << "Invalid default vendor for Apple drive";
	EXPECT_EQ(string(rascsi_get_version_string()).substr(0, 2) + string(rascsi_get_version_string()).substr(3, 2),
			device->GetRevision());

	EXPECT_EQ(32, device->GetSupportedLuns());
}

TEST(DeviceTest, SCRM_Device_Defaults)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "test.hdr");
	EXPECT_NE(nullptr, device);
	EXPECT_EQ(true, device->IsProtectable());
	EXPECT_EQ(false, device->IsProtected());
	EXPECT_EQ(false, device->IsReadOnly());
	EXPECT_EQ(true, device->IsRemovable());
	EXPECT_EQ(false, device->IsRemoved());
	EXPECT_EQ(true, device->IsLockable());
	EXPECT_EQ(false, device->IsLocked());
	EXPECT_EQ(true, device->IsStoppable());
	EXPECT_EQ(false, device->IsStopped());

	EXPECT_EQ("RaSCSI", device->GetVendor());
	EXPECT_EQ("SCSI HD (REM.)", device->GetProduct());
	EXPECT_EQ(string(rascsi_get_version_string()).substr(0, 2) + string(rascsi_get_version_string()).substr(3, 2),
			device->GetRevision());

	EXPECT_EQ(32, device->GetSupportedLuns());
}

TEST(DeviceTest, SCMO_Device_Defaults)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "test.mos");
	EXPECT_NE(nullptr, device);
	EXPECT_EQ(true, device->IsProtectable());
	EXPECT_EQ(false, device->IsProtected());
	EXPECT_EQ(false, device->IsReadOnly());
	EXPECT_EQ(true, device->IsRemovable());
	EXPECT_EQ(false, device->IsRemoved());
	EXPECT_EQ(true, device->IsLockable());
	EXPECT_EQ(false, device->IsLocked());
	EXPECT_EQ(true, device->IsStoppable());
	EXPECT_EQ(false, device->IsStopped());

	EXPECT_EQ("RaSCSI", device->GetVendor());
	EXPECT_EQ("SCSI MO", device->GetProduct());
	EXPECT_EQ(string(rascsi_get_version_string()).substr(0, 2) + string(rascsi_get_version_string()).substr(3, 2),
			device->GetRevision());

	EXPECT_EQ(32, device->GetSupportedLuns());
}

TEST(DeviceTest, SCCD_Device_Defaults)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "test.iso");
	EXPECT_NE(nullptr, device);
	EXPECT_EQ(false, device->IsProtectable());
	EXPECT_EQ(false, device->IsProtected());
	EXPECT_EQ(true, device->IsReadOnly());
	EXPECT_EQ(true, device->IsRemovable());
	EXPECT_EQ(false, device->IsRemoved());
	EXPECT_EQ(true, device->IsLockable());
	EXPECT_EQ(false, device->IsLocked());
	EXPECT_EQ(true, device->IsStoppable());
	EXPECT_EQ(false, device->IsStopped());

	EXPECT_EQ("RaSCSI", device->GetVendor());
	EXPECT_EQ("SCSI CD-ROM", device->GetProduct());
	EXPECT_EQ(string(rascsi_get_version_string()).substr(0, 2) + string(rascsi_get_version_string()).substr(3, 2),
			device->GetRevision());

	EXPECT_EQ(32, device->GetSupportedLuns());
}

}
