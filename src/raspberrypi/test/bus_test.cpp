//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gmock/gmock.h>

#include "bus.h"

TEST(BusTest, GetPhase)
{
	EXPECT_EQ(BUS::phase_t::dataout, BUS::GetPhase(0b000));
	EXPECT_EQ(BUS::phase_t::datain, BUS::GetPhase(0b001));
	EXPECT_EQ(BUS::phase_t::command, BUS::GetPhase(0b010));
	EXPECT_EQ(BUS::phase_t::status, BUS::GetPhase(0b011));
	EXPECT_EQ(BUS::phase_t::reserved, BUS::GetPhase(0b100));
	EXPECT_EQ(BUS::phase_t::reserved, BUS::GetPhase(0b101));
	EXPECT_EQ(BUS::phase_t::msgout, BUS::GetPhase(0b110));
	EXPECT_EQ(BUS::phase_t::msgin, BUS::GetPhase(0b111));
}
