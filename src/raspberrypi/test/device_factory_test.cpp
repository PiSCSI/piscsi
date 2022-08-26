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

DeviceFactory& device_factory = DeviceFactory::instance();

TEST(DeviceFactoryTest, GetTypeForFile)
{
	EXPECT_EQ(SAHD, device_factory.GetTypeForFile("test.hdf"));
	EXPECT_EQ(SCHD, device_factory.GetTypeForFile("test.hds"));
	EXPECT_EQ(SCHD, device_factory.GetTypeForFile("test.HDS"));
	EXPECT_EQ(SCHD, device_factory.GetTypeForFile("test.hda"));
	EXPECT_EQ(SCHD, device_factory.GetTypeForFile("test.hdn"));
	EXPECT_EQ(SCHD, device_factory.GetTypeForFile("test.hdi"));
	EXPECT_EQ(SCHD, device_factory.GetTypeForFile("test.nhd"));
	EXPECT_EQ(SCRM, device_factory.GetTypeForFile("test.hdr"));
	EXPECT_EQ(SCMO, device_factory.GetTypeForFile("test.mos"));
	EXPECT_EQ(SCCD, device_factory.GetTypeForFile("test.iso"));
	EXPECT_EQ(SCCD, device_factory.GetTypeForFile("test.suffix.iso"));
	EXPECT_EQ(SCBR, device_factory.GetTypeForFile("bridge"));
	EXPECT_EQ(SCDP, device_factory.GetTypeForFile("daynaport"));
	EXPECT_EQ(SCLP, device_factory.GetTypeForFile("printer"));
	EXPECT_EQ(SCHS, device_factory.GetTypeForFile("services"));
	EXPECT_EQ(UNDEFINED, device_factory.GetTypeForFile("unknown"));
	EXPECT_EQ(UNDEFINED, device_factory.GetTypeForFile("test.iso.suffix"));
}
