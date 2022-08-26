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
}
