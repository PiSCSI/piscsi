//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "hal/bus.h"
#include "mocks.h"

TEST(BusTest, GetCommandByteCount)
{
    EXPECT_EQ(41, scsi_defs::command_mapping.size());
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x00));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x01));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x03));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x04));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x07));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x08));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x09));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x0a));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x0b));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x0c));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x0d));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x0e));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x10));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x12));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x15));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x16));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x17));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x1a));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x1b));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x1d));
    EXPECT_EQ(6, BUS::GetCommandByteCount(0x1e));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x25));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x28));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x2a));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x2b));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x2f));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x35));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x37));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x3e));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x3f));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x43));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x4a));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x55));
    EXPECT_EQ(10, BUS::GetCommandByteCount(0x5a));
    EXPECT_EQ(12, BUS::GetCommandByteCount(0xa0));
    EXPECT_EQ(16, BUS::GetCommandByteCount(0x88));
    EXPECT_EQ(16, BUS::GetCommandByteCount(0x8a));
    EXPECT_EQ(16, BUS::GetCommandByteCount(0x8f));
    EXPECT_EQ(16, BUS::GetCommandByteCount(0x91));
    EXPECT_EQ(16, BUS::GetCommandByteCount(0x9e));
    EXPECT_EQ(16, BUS::GetCommandByteCount(0x9f));
    EXPECT_EQ(0, BUS::GetCommandByteCount(0x1f));
}

TEST(BusTest, GetPhase)
{
    EXPECT_EQ(phase_t::dataout, BUS::GetPhase(0b000));
    EXPECT_EQ(phase_t::datain, BUS::GetPhase(0b001));
    EXPECT_EQ(phase_t::command, BUS::GetPhase(0b010));
    EXPECT_EQ(phase_t::status, BUS::GetPhase(0b011));
    EXPECT_EQ(phase_t::reserved, BUS::GetPhase(0b100));
    EXPECT_EQ(phase_t::reserved, BUS::GetPhase(0b101));
    EXPECT_EQ(phase_t::msgout, BUS::GetPhase(0b110));
    EXPECT_EQ(phase_t::msgin, BUS::GetPhase(0b111));

    NiceMock<MockBus> bus;

    EXPECT_EQ(phase_t::busfree, bus.GetPhase());

    ON_CALL(bus, GetSEL()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::selection, bus.GetPhase());

    ON_CALL(bus, GetSEL()).WillByDefault(Return(false));
    ON_CALL(bus, GetBSY()).WillByDefault(Return(true));

    ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
    EXPECT_EQ(phase_t::dataout, bus.GetPhase());
    ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::reserved, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
    ON_CALL(bus, GetCD()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::command, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
    ON_CALL(bus, GetCD()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::msgout, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
    ON_CALL(bus, GetCD()).WillByDefault(Return(false));
    ON_CALL(bus, GetIO()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::datain, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
    ON_CALL(bus, GetCD()).WillByDefault(Return(false));
    ON_CALL(bus, GetIO()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::reserved, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(true));
    ON_CALL(bus, GetCD()).WillByDefault(Return(true));
    ON_CALL(bus, GetIO()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::msgin, bus.GetPhase());

    ON_CALL(bus, GetMSG()).WillByDefault(Return(false));
    ON_CALL(bus, GetCD()).WillByDefault(Return(true));
    ON_CALL(bus, GetIO()).WillByDefault(Return(true));
    EXPECT_EQ(phase_t::status, bus.GetPhase());
}

TEST(BusTest, GetPhaseStrRaw)
{
    EXPECT_STREQ("busfree", BUS::GetPhaseStrRaw(phase_t::busfree));
    EXPECT_STREQ("arbitration", BUS::GetPhaseStrRaw(phase_t::arbitration));
    EXPECT_STREQ("selection", BUS::GetPhaseStrRaw(phase_t::selection));
    EXPECT_STREQ("reselection", BUS::GetPhaseStrRaw(phase_t::reselection));
    EXPECT_STREQ("command", BUS::GetPhaseStrRaw(phase_t::command));
    EXPECT_STREQ("datain", BUS::GetPhaseStrRaw(phase_t::datain));
    EXPECT_STREQ("dataout", BUS::GetPhaseStrRaw(phase_t::dataout));
    EXPECT_STREQ("status", BUS::GetPhaseStrRaw(phase_t::status));
    EXPECT_STREQ("msgin", BUS::GetPhaseStrRaw(phase_t::msgin));
    EXPECT_STREQ("msgout", BUS::GetPhaseStrRaw(phase_t::msgout));
    EXPECT_STREQ("reserved", BUS::GetPhaseStrRaw(phase_t::reserved));
}
