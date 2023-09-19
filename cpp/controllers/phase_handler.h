//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/scsi.h"
#include "shared/piscsi_exceptions.h"
#include <unordered_map>
#include <functional>

using namespace scsi_defs;

class PhaseHandler
{
	phase_t phase = phase_t::busfree;

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

	virtual phase_t Process(int) = 0;

protected:

	phase_t GetPhase() const { return phase; }
	void SetPhase(phase_t p) { phase = p; }
	bool IsSelection() const { return phase == phase_t::selection; }
	bool IsBusFree() const { return phase == phase_t::busfree; }
	bool IsCommand() const { return phase == phase_t::command; }
	bool IsStatus() const { return phase == phase_t::status; }
	bool IsDataIn() const { return phase == phase_t::datain; }
	bool IsDataOut() const { return phase == phase_t::dataout; }
	bool IsMsgIn() const { return phase == phase_t::msgin; }
	bool IsMsgOut() const { return phase == phase_t::msgout; }

	void ProcessPhase() const
	{
		try {
			phase_executors.at(phase)();
		}
		catch(const out_of_range&) {
			throw scsi_exception(sense_key::aborted_command);
		}
	}

private:

	const unordered_map<phase_t, function<void()>> phase_executors = {
			{ phase_t::busfree, [this] () { BusFree(); } },
			{ phase_t::selection, [this] () { Selection(); } },
			{ phase_t::dataout, [this] () { DataOut(); } },
			{ phase_t::datain, [this] () { DataIn(); } },
			{ phase_t::command, [this] () { Command(); } },
			{ phase_t::status, [this] () { Status(); } },
			{ phase_t::msgout, [this] () { MsgOut(); } },
			{ phase_t::msgin, [this] () { MsgIn(); } },
	};
};
