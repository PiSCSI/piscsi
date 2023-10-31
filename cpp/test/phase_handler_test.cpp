//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
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

TEST(PhaseHandlerTest, ProcessPhase)
{
	MockPhaseHandler handler;
	handler.Init();

	handler.SetPhase(phase_t::selection);
	EXPECT_CALL(handler, Selection);
	handler.ProcessPhase();

	handler.SetPhase(phase_t::busfree);
	EXPECT_CALL(handler, BusFree);
	handler.ProcessPhase();

	handler.SetPhase(phase_t::datain);
	EXPECT_CALL(handler, DataIn);
	handler.ProcessPhase();

	handler.SetPhase(phase_t::dataout);
	EXPECT_CALL(handler, DataOut);
	handler.ProcessPhase();

	handler.SetPhase(phase_t::command);
	EXPECT_CALL(handler, Command);
	handler.ProcessPhase();

	handler.SetPhase(phase_t::status);
	EXPECT_CALL(handler, Status);
	handler.ProcessPhase();

	handler.SetPhase(phase_t::msgin);
	EXPECT_CALL(handler, MsgIn);
	handler.ProcessPhase();

	handler.SetPhase(phase_t::msgout);
	EXPECT_CALL(handler, MsgOut);
	handler.ProcessPhase();

	handler.SetPhase(phase_t::reselection);
	EXPECT_THAT([&] { handler.ProcessPhase(); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::aborted_command),
			Property(&scsi_exception::get_asc, asc::no_additional_sense_information))));

	handler.SetPhase(phase_t::reserved);
	EXPECT_THAT([&] { handler.ProcessPhase(); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::aborted_command),
			Property(&scsi_exception::get_asc, asc::no_additional_sense_information))));
}
