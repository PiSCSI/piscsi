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

	MockBus bus;

	EXPECT_EQ(BUS::phase_t::busfree, bus.GetPhase());

	ON_CALL(bus, GetSEL()).WillByDefault(testing::Return(true));
	EXPECT_EQ(BUS::phase_t::selection, bus.GetPhase());

	ON_CALL(bus, GetSEL()).WillByDefault(testing::Return(false));
	ON_CALL(bus, GetBSY()).WillByDefault(testing::Return(true));

	ON_CALL(bus, GetMSG()).WillByDefault(testing::Return(false));
	EXPECT_EQ(BUS::phase_t::dataout, bus.GetPhase());
	ON_CALL(bus, GetMSG()).WillByDefault(testing::Return(true));
	EXPECT_EQ(BUS::phase_t::reserved, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(testing::Return(false));
	ON_CALL(bus, GetCD()).WillByDefault(testing::Return(true));
	EXPECT_EQ(BUS::phase_t::command, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(testing::Return(true));
	ON_CALL(bus, GetCD()).WillByDefault(testing::Return(true));
	EXPECT_EQ(BUS::phase_t::msgout, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(testing::Return(false));
	ON_CALL(bus, GetCD()).WillByDefault(testing::Return(false));
	ON_CALL(bus, GetIO()).WillByDefault(testing::Return(true));
	EXPECT_EQ(BUS::phase_t::datain, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(testing::Return(true));
	ON_CALL(bus, GetCD()).WillByDefault(testing::Return(false));
	ON_CALL(bus, GetIO()).WillByDefault(testing::Return(true));
	EXPECT_EQ(BUS::phase_t::reserved, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(testing::Return(true));
	ON_CALL(bus, GetCD()).WillByDefault(testing::Return(true));
	ON_CALL(bus, GetIO()).WillByDefault(testing::Return(true));
	EXPECT_EQ(BUS::phase_t::msgin, bus.GetPhase());

	ON_CALL(bus, GetMSG()).WillByDefault(testing::Return(false));
	ON_CALL(bus, GetCD()).WillByDefault(testing::Return(true));
	ON_CALL(bus, GetIO()).WillByDefault(testing::Return(true));
	EXPECT_EQ(BUS::phase_t::status, bus.GetPhase());
}

TEST(BusTest, GetPinRaw)
{
	EXPECT_EQ(0, BUS::GetPinRaw(0, 0));
	EXPECT_EQ(0, BUS::GetPinRaw(0, 7));
	EXPECT_EQ(1, BUS::GetPinRaw(-1, 0));
	EXPECT_EQ(1, BUS::GetPinRaw(-1, 7));
}
