//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
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

	NiceMock<MockBus> bus;

	EXPECT_EQ(BUS::phase_t::busfree, bus.GetPhase());

	ON_CALL(bus, GetSEL()).WillByDefault(Return(true));
	EXPECT_EQ(BUS::phase_t::selection, bus.GetPhase());

	ON_CALL(bus, GetSEL()).WillByDefault(Return(false));
	ON_CALL(bus, GetBSY()).WillByDefault(Return(true));

	ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
	EXPECT_EQ(BUS::phase_t::dataout, bus.GetPhase());
	ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
	EXPECT_EQ(BUS::phase_t::reserved, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
	ON_CALL(bus, GetCD()).WillByDefault(Return(true));
	EXPECT_EQ(BUS::phase_t::command, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
	ON_CALL(bus, GetCD()).WillByDefault(Return(true));
	EXPECT_EQ(BUS::phase_t::msgout, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
	ON_CALL(bus, GetCD()).WillByDefault(Return(false));
	ON_CALL(bus, GetIO()).WillByDefault(Return(true));
	EXPECT_EQ(BUS::phase_t::datain, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
	ON_CALL(bus, GetCD()).WillByDefault(Return(false));
	ON_CALL(bus, GetIO()).WillByDefault(Return(true));
	EXPECT_EQ(BUS::phase_t::reserved, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
	ON_CALL(bus, GetCD()).WillByDefault(Return(true));
	ON_CALL(bus, GetIO()).WillByDefault(Return(true));
	EXPECT_EQ(BUS::phase_t::msgin, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
	ON_CALL(bus, GetCD()).WillByDefault(Return(true));
	ON_CALL(bus, GetIO()).WillByDefault(Return(true));
	EXPECT_EQ(BUS::phase_t::status, bus.GetPhase());
}

TEST(BusTest, GetPhaseStrRaw)
{
	EXPECT_STREQ("busfree", BUS::GetPhaseStrRaw(BUS::phase_t::busfree));
	EXPECT_STREQ("arbitration", BUS::GetPhaseStrRaw(BUS::phase_t::arbitration));
	EXPECT_STREQ("selection", BUS::GetPhaseStrRaw(BUS::phase_t::selection));
	EXPECT_STREQ("reselection", BUS::GetPhaseStrRaw(BUS::phase_t::reselection));
	EXPECT_STREQ("command", BUS::GetPhaseStrRaw(BUS::phase_t::command));
	EXPECT_STREQ("datain", BUS::GetPhaseStrRaw(BUS::phase_t::datain));
	EXPECT_STREQ("dataout", BUS::GetPhaseStrRaw(BUS::phase_t::dataout));
	EXPECT_STREQ("status", BUS::GetPhaseStrRaw(BUS::phase_t::status));
	EXPECT_STREQ("msgin", BUS::GetPhaseStrRaw(BUS::phase_t::msgin));
	EXPECT_STREQ("msgout", BUS::GetPhaseStrRaw(BUS::phase_t::msgout));
	EXPECT_STREQ("reserved", BUS::GetPhaseStrRaw(BUS::phase_t::reserved));
}

TEST(BusTest, GetPinRaw)
{
	EXPECT_EQ(0, BUS::GetPinRaw(0, 0));
	EXPECT_EQ(0, BUS::GetPinRaw(0, 7));
	EXPECT_EQ(1, BUS::GetPinRaw(-1, 0));
	EXPECT_EQ(1, BUS::GetPinRaw(-1, 7));
}
