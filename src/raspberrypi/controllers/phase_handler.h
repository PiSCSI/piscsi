//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi.h"

class PhaseHandler
{
	BUS::phase_t phase = BUS::phase_t::busfree;

public:

	PhaseHandler() = default;
	virtual ~PhaseHandler() = default;

	virtual void BusFree() = 0;
	virtual void Selection() = 0;
	virtual void Command() = 0;
	virtual void Status() = 0;
	virtual void DataIn() = 0;
	virtual void DataOut() = 0;
	virtual void MsgIn() = 0;
	virtual void MsgOut() = 0;

	virtual BUS::phase_t Process(int) = 0;

protected:

	BUS::phase_t GetPhase() const { return phase; }
	void SetPhase(BUS::phase_t p) { phase = p; }
	bool IsSelection() const { return phase == BUS::phase_t::selection; }
	bool IsBusFree() const { return phase == BUS::phase_t::busfree; }
	bool IsCommand() const { return phase == BUS::phase_t::command; }
	bool IsStatus() const { return phase == BUS::phase_t::status; }
	bool IsDataIn() const { return phase == BUS::phase_t::datain; }
	bool IsDataOut() const { return phase == BUS::phase_t::dataout; }
	bool IsMsgIn() const { return phase == BUS::phase_t::msgin; }
	bool IsMsgOut() const { return phase == BUS::phase_t::msgout; }
};
