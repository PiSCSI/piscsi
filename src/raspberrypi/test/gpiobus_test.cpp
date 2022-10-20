//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "hal/gpiobus.h"
#include <set>

TEST(GpioBusTest, GetCommandByteCount)
{
	set<BYTE> opcodes;

	EXPECT_EQ(16, GPIOBUS::GetCommandByteCount(0x88));
	opcodes.insert(0x88);
	EXPECT_EQ(16, GPIOBUS::GetCommandByteCount(0x8a));
	opcodes.insert(0x8a);
	EXPECT_EQ(16, GPIOBUS::GetCommandByteCount(0x8f));
	opcodes.insert(0x8f);
	EXPECT_EQ(16, GPIOBUS::GetCommandByteCount(0x91));
	opcodes.insert(0x91);
	EXPECT_EQ(16, GPIOBUS::GetCommandByteCount(0x9e));
	opcodes.insert(0x9e);
	EXPECT_EQ(16, GPIOBUS::GetCommandByteCount(0x9f));
	opcodes.insert(0x9f);
	EXPECT_EQ(12, GPIOBUS::GetCommandByteCount(0xa0));
	opcodes.insert(0xa0);

	// TODO Opcode 0x05 must be removed from gpiobus.cpp
	EXPECT_EQ(10, GPIOBUS::GetCommandByteCount(0x05));
	opcodes.insert(0x05);

	for (int i = 0x20; i <= 0x7d; i++) {
		EXPECT_EQ(10, GPIOBUS::GetCommandByteCount(i));
		opcodes.insert(i);
	}

	for (int i = 0; i < 256; i++) {
		if (opcodes.find(i) == opcodes.end()) {
			EXPECT_EQ(6, GPIOBUS::GetCommandByteCount(i));
		}
	}
}
