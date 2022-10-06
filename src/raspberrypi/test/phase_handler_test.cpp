//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "controllers/phase_handler.h"

TEST(PhaseHandlerTest, Phases)
{
	MockPhaseHandler handler;

	handler.SetPhase(BUS::phase_t::selection);
	EXPECT_TRUE(handler.IsSelection());

	handler.SetPhase(BUS::phase_t::busfree);
	EXPECT_TRUE(handler.IsBusFree());

	handler.SetPhase(BUS::phase_t::command);
	EXPECT_TRUE(handler.IsCommand());

	handler.SetPhase(BUS::phase_t::status);
	EXPECT_TRUE(handler.IsStatus());

	handler.SetPhase(BUS::phase_t::datain);
	EXPECT_TRUE(handler.IsDataIn());

	handler.SetPhase(BUS::phase_t::dataout);
	EXPECT_TRUE(handler.IsDataOut());

	handler.SetPhase(BUS::phase_t::msgin);
	EXPECT_TRUE(handler.IsMsgIn());

	handler.SetPhase(BUS::phase_t::msgout);
	EXPECT_TRUE(handler.IsMsgOut());
}
