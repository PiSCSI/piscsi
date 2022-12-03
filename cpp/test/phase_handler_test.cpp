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

	handler.SetPhase(phase_t::selection);
	EXPECT_TRUE(handler.IsSelection());
	EXPECT_FALSE(handler.IsBusFree());
	EXPECT_FALSE(handler.IsCommand());
	EXPECT_FALSE(handler.IsStatus());
	EXPECT_FALSE(handler.IsDataIn());
	EXPECT_FALSE(handler.IsDataOut());
	EXPECT_FALSE(handler.IsMsgIn());
	EXPECT_FALSE(handler.IsMsgOut());

	handler.SetPhase(phase_t::busfree);
	EXPECT_TRUE(handler.IsBusFree());
	EXPECT_FALSE(handler.IsSelection());
	EXPECT_FALSE(handler.IsCommand());
	EXPECT_FALSE(handler.IsStatus());
	EXPECT_FALSE(handler.IsDataIn());
	EXPECT_FALSE(handler.IsDataOut());
	EXPECT_FALSE(handler.IsMsgIn());
	EXPECT_FALSE(handler.IsMsgOut());

	handler.SetPhase(phase_t::command);
	EXPECT_TRUE(handler.IsCommand());
	EXPECT_FALSE(handler.IsBusFree());
	EXPECT_FALSE(handler.IsSelection());
	EXPECT_FALSE(handler.IsStatus());
	EXPECT_FALSE(handler.IsDataIn());
	EXPECT_FALSE(handler.IsDataOut());
	EXPECT_FALSE(handler.IsMsgIn());
	EXPECT_FALSE(handler.IsMsgOut());

	handler.SetPhase(phase_t::status);
	EXPECT_TRUE(handler.IsStatus());
	EXPECT_FALSE(handler.IsBusFree());
	EXPECT_FALSE(handler.IsSelection());
	EXPECT_FALSE(handler.IsCommand());
	EXPECT_FALSE(handler.IsDataIn());
	EXPECT_FALSE(handler.IsDataOut());
	EXPECT_FALSE(handler.IsMsgIn());
	EXPECT_FALSE(handler.IsMsgOut());

	handler.SetPhase(phase_t::datain);
	EXPECT_TRUE(handler.IsDataIn());
	EXPECT_FALSE(handler.IsBusFree());
	EXPECT_FALSE(handler.IsSelection());
	EXPECT_FALSE(handler.IsCommand());
	EXPECT_FALSE(handler.IsStatus());
	EXPECT_FALSE(handler.IsDataOut());
	EXPECT_FALSE(handler.IsMsgIn());
	EXPECT_FALSE(handler.IsMsgOut());

	handler.SetPhase(phase_t::dataout);
	EXPECT_TRUE(handler.IsDataOut());
	EXPECT_FALSE(handler.IsBusFree());
	EXPECT_FALSE(handler.IsSelection());
	EXPECT_FALSE(handler.IsCommand());
	EXPECT_FALSE(handler.IsStatus());
	EXPECT_FALSE(handler.IsDataIn());
	EXPECT_FALSE(handler.IsMsgIn());
	EXPECT_FALSE(handler.IsMsgOut());

	handler.SetPhase(phase_t::msgin);
	EXPECT_TRUE(handler.IsMsgIn());
	EXPECT_FALSE(handler.IsBusFree());
	EXPECT_FALSE(handler.IsSelection());
	EXPECT_FALSE(handler.IsCommand());
	EXPECT_FALSE(handler.IsStatus());
	EXPECT_FALSE(handler.IsDataIn());
	EXPECT_FALSE(handler.IsDataOut());
	EXPECT_FALSE(handler.IsMsgOut());

	handler.SetPhase(phase_t::msgout);
	EXPECT_TRUE(handler.IsMsgOut());
	EXPECT_FALSE(handler.IsBusFree());
	EXPECT_FALSE(handler.IsSelection());
	EXPECT_FALSE(handler.IsCommand());
	EXPECT_FALSE(handler.IsStatus());
	EXPECT_FALSE(handler.IsDataIn());
	EXPECT_FALSE(handler.IsDataOut());
	EXPECT_FALSE(handler.IsMsgIn());
}
