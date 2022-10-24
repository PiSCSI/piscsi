//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// These tests only test up the point where a network connection is required.
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "rascsi/rascsi_service.h"

TEST(RascsiServiceTest, LifeCycle)
{
	RascsiService service;

	EXPECT_TRUE(service.Init(nullptr, 65535));
	EXPECT_FALSE(service.Init(nullptr, 65536));
	EXPECT_FALSE(service.Init(nullptr, 0));
	EXPECT_FALSE(service.Init(nullptr, -1));

	service.Cleanup();
}
