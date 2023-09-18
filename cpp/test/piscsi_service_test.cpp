//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// These tests only test up the point where a network connection is required.
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "piscsi/piscsi_service.h"

TEST(PiscsiServiceTest, Init)
{
	PiscsiService service;

	EXPECT_FALSE(service.Init(nullptr, 65536)) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, 0)) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, -1)) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, 1)) << "Port 1 is only available for the root user";
	EXPECT_TRUE(service.Init(nullptr, 9999));
}

TEST(PiscsiServiceTest, StartStop)
{
	PiscsiService service;

	EXPECT_TRUE(service.Init(nullptr, 9999));

	service.Start();
	service.Stop();
}
