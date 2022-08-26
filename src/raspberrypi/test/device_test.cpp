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
	EXPECT_EQ(true, device->SupportsFile());
	EXPECT_EQ(false, device->SupportsParams());
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
	EXPECT_EQ(true, device->SupportsFile());
	EXPECT_EQ(false, device->SupportsParams());
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
	EXPECT_EQ(true, device->SupportsFile());
	EXPECT_EQ(false, device->SupportsParams());
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
	EXPECT_EQ(true, device->SupportsFile());
	EXPECT_EQ(false, device->SupportsParams());
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

TEST(DeviceTest, SCBR_Device_Defaults)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "bridge");
	EXPECT_NE(nullptr, device);
	EXPECT_EQ(false, device->SupportsFile());
	EXPECT_EQ(true, device->SupportsParams());
	EXPECT_EQ(false, device->IsProtectable());
	EXPECT_EQ(false, device->IsProtected());
	EXPECT_EQ(false, device->IsReadOnly());
	EXPECT_EQ(false, device->IsRemovable());
	EXPECT_EQ(false, device->IsRemoved());
	EXPECT_EQ(false, device->IsLockable());
	EXPECT_EQ(false, device->IsLocked());
	EXPECT_EQ(false, device->IsStoppable());
	EXPECT_EQ(false, device->IsStopped());

	EXPECT_EQ("RaSCSI", device->GetVendor());
	EXPECT_EQ("SCSI HOST BRIDGE", device->GetProduct());
	EXPECT_EQ(string(rascsi_get_version_string()).substr(0, 2) + string(rascsi_get_version_string()).substr(3, 2),
			device->GetRevision());

	EXPECT_EQ(32, device->GetSupportedLuns());
}

TEST(DeviceTest, SCDP_Device_Defaults)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "daynaport");
	EXPECT_NE(nullptr, device);
	EXPECT_EQ(false, device->SupportsFile());
	EXPECT_EQ(true, device->SupportsParams());
	EXPECT_EQ(false, device->IsProtectable());
	EXPECT_EQ(false, device->IsProtected());
	EXPECT_EQ(false, device->IsReadOnly());
	EXPECT_EQ(false, device->IsRemovable());
	EXPECT_EQ(false, device->IsRemoved());
	EXPECT_EQ(false, device->IsLockable());
	EXPECT_EQ(false, device->IsLocked());
	EXPECT_EQ(false, device->IsStoppable());
	EXPECT_EQ(false, device->IsStopped());

	EXPECT_EQ("Dayna", device->GetVendor());
	EXPECT_EQ("SCSI/Link", device->GetProduct());
	EXPECT_EQ("1.4a", device->GetRevision());

	EXPECT_EQ(32, device->GetSupportedLuns());
}

TEST(DeviceTest, SCHS_Device_Defaults)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "services");
	EXPECT_NE(nullptr, device);
	// TODO Neither parameters nor files are supported.
	// SupportsFile() returns true, whic is a bug which may also require client changes after fixing.
	EXPECT_EQ(true, device->SupportsFile());
	EXPECT_EQ(false, device->SupportsParams());
	EXPECT_EQ(false, device->IsProtectable());
	EXPECT_EQ(false, device->IsProtected());
	EXPECT_EQ(false, device->IsReadOnly());
	EXPECT_EQ(false, device->IsRemovable());
	EXPECT_EQ(false, device->IsRemoved());
	EXPECT_EQ(false, device->IsLockable());
	EXPECT_EQ(false, device->IsLocked());
	EXPECT_EQ(false, device->IsStoppable());
	EXPECT_EQ(false, device->IsStopped());

	EXPECT_EQ("RaSCSI", device->GetVendor());
	EXPECT_EQ("Host Services", device->GetProduct());
	EXPECT_EQ(string(rascsi_get_version_string()).substr(0, 2) + string(rascsi_get_version_string()).substr(3, 2),
			device->GetRevision());

	EXPECT_EQ(32, device->GetSupportedLuns());
}

TEST(DeviceTest, SCLP_Device_Defaults)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "printer");
	EXPECT_NE(nullptr, device);
	EXPECT_EQ(false, device->SupportsFile());
	EXPECT_EQ(true, device->SupportsParams());
	EXPECT_EQ(false, device->IsProtectable());
	EXPECT_EQ(false, device->IsProtected());
	EXPECT_EQ(false, device->IsReadOnly());
	EXPECT_EQ(false, device->IsRemovable());
	EXPECT_EQ(false, device->IsRemoved());
	EXPECT_EQ(false, device->IsLockable());
	EXPECT_EQ(false, device->IsLocked());
	EXPECT_EQ(false, device->IsStoppable());
	EXPECT_EQ(false, device->IsStopped());

	EXPECT_EQ("RaSCSI", device->GetVendor());
	EXPECT_EQ("SCSI PRINTER", device->GetProduct());
	EXPECT_EQ(string(rascsi_get_version_string()).substr(0, 2) + string(rascsi_get_version_string()).substr(3, 2),
			device->GetRevision());

	EXPECT_EQ(32, device->GetSupportedLuns());
}

}
