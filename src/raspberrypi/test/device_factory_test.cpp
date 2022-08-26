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

namespace DeviceFactoryTest
{

DeviceFactory& device_factory = DeviceFactory::instance();

TEST(DeviceFactoryTest, GetTypeForFile)
{
	EXPECT_EQ(device_factory.GetTypeForFile("test.hdf"), SAHD);
	EXPECT_EQ(device_factory.GetTypeForFile("test.hds"), SCHD);
	EXPECT_EQ(device_factory.GetTypeForFile("test.HDS"), SCHD);
	EXPECT_EQ(device_factory.GetTypeForFile("test.hda"), SCHD);
	EXPECT_EQ(device_factory.GetTypeForFile("test.hdn"), SCHD);
	EXPECT_EQ(device_factory.GetTypeForFile("test.hdi"), SCHD);
	EXPECT_EQ(device_factory.GetTypeForFile("test.nhd"), SCHD);
	EXPECT_EQ(device_factory.GetTypeForFile("test.hdr"), SCRM);
	EXPECT_EQ(device_factory.GetTypeForFile("test.mos"), SCMO);
	EXPECT_EQ(device_factory.GetTypeForFile("test.iso"), SCCD);
	EXPECT_EQ(device_factory.GetTypeForFile("test.suffix.iso"), SCCD);
	EXPECT_EQ(device_factory.GetTypeForFile("bridge"), SCBR);
	EXPECT_EQ(device_factory.GetTypeForFile("daynaport"), SCDP);
	EXPECT_EQ(device_factory.GetTypeForFile("printer"), SCLP);
	EXPECT_EQ(device_factory.GetTypeForFile("services"), SCHS);
	EXPECT_EQ(device_factory.GetTypeForFile("unknown"), UNDEFINED);
	EXPECT_EQ(device_factory.GetTypeForFile("test.iso.suffix"), UNDEFINED);
}

TEST(DeviceFactoryTest, GetSectorSizes)
{
	unordered_set<uint32_t> sector_sizes;

	sector_sizes = device_factory.GetSectorSizes(SCHD);
	EXPECT_EQ(4, sector_sizes.size());
	EXPECT_EQ(true, sector_sizes.find(512) != sector_sizes.end());
	EXPECT_EQ(true, sector_sizes.find(1024) != sector_sizes.end());
	EXPECT_EQ(true, sector_sizes.find(2048) != sector_sizes.end());
	EXPECT_EQ(true, sector_sizes.find(4096) != sector_sizes.end());

	sector_sizes = device_factory.GetSectorSizes(SCRM);
	EXPECT_EQ(4, sector_sizes.size());
	EXPECT_EQ(true, sector_sizes.find(512) != sector_sizes.end());
	EXPECT_EQ(true, sector_sizes.find(1024) != sector_sizes.end());
	EXPECT_EQ(true, sector_sizes.find(2048) != sector_sizes.end());
	EXPECT_EQ(true, sector_sizes.find(4096) != sector_sizes.end());

	sector_sizes = device_factory.GetSectorSizes(SCMO);
	EXPECT_EQ(4, sector_sizes.size());
	EXPECT_EQ(true, sector_sizes.find(512) != sector_sizes.end());
	EXPECT_EQ(true, sector_sizes.find(1024) != sector_sizes.end());
	EXPECT_EQ(true, sector_sizes.find(2048) != sector_sizes.end());
	EXPECT_EQ(true, sector_sizes.find(4096) != sector_sizes.end());

	sector_sizes = device_factory.GetSectorSizes(SCCD);
	EXPECT_EQ(2, sector_sizes.size());
	EXPECT_EQ(true, sector_sizes.find(512) != sector_sizes.end());
	EXPECT_EQ(true, sector_sizes.find(2048) != sector_sizes.end());
}

TEST(DeviceFactoryTest, UnknownDeviceType)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "test");
	EXPECT_EQ(nullptr, device);
}

TEST(DeviceFactoryTest, SCHD_Device_Defaults)
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

TEST(DeviceFactoryTest, SCRM_Device_Defaults)
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

TEST(DeviceFactoryTest, SCMO_Device_Defaults)
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

TEST(DeviceFactoryTest, SCCD_Device_Defaults)
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

TEST(DeviceFactoryTest, SCBR_Device_Defaults)
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

TEST(DeviceFactoryTest, SCDP_Device_Defaults)
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

TEST(DeviceFactoryTest, SCHS_Device_Defaults)
{
	Device *device = device_factory.CreateDevice(UNDEFINED, "services");
	EXPECT_NE(nullptr, device);
	EXPECT_EQ(false, device->SupportsFile());
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

TEST(DeviceFactoryTest, SCLP_Device_Defaults)
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
