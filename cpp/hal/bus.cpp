//---------------------------------------------------------------------------
//
// X68000 EMULATOR "XM6"
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022 Uwe Seimet
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "bus.h"

using namespace std;
using namespace scsi_defs;

//---------------------------------------------------------------------------
//
// Get the number of bytes for a command
//
//---------------------------------------------------------------------------
int BUS::GetCommandByteCount(uint8_t opcode)
{
	const auto& mapping = command_mapping.find(static_cast<scsi_command>(opcode));

	return mapping != command_mapping.end() ? mapping->second.first : 0;
}

//---------------------------------------------------------------------------
//
//	Phase Acquisition
//
//---------------------------------------------------------------------------
phase_t BUS::GetPhase()
{
	// Selection Phase
	if (GetSEL()) {
		return phase_t::selection;
	}

	// Bus busy phase
	if (!GetBSY()) {
		return phase_t::busfree;
	}

	// Get target phase from bus signal line
	int mci = GetMSG() ? 0b100 : 0b000;
	mci |= GetCD() ? 0b010 : 0b000;
	mci |= GetIO() ? 0b001 : 0b000;
	return GetPhase(mci);
}

//---------------------------------------------------------------------------
//
//	Determine Phase String phase enum
//
//---------------------------------------------------------------------------
const char* BUS::GetPhaseStrRaw(phase_t current_phase) {
	const auto& it = phase_str_mapping.find(current_phase);
	return it != phase_str_mapping.end() ? it->second : "INVALID";
}

// Phase Table
// Reference Table 8: https://www.staff.uni-mainz.de/tacke/scsi/SCSI2-06.html
// This determines the phase based upon the Msg, C/D and I/O signals.
//
// |MSG|C/D|I/O| Phase
// | 0 | 0 | 0 | DATA OUT
// | 0 | 0 | 1 | DATA IN
// | 0 | 1 | 0 | COMMAND
// | 0 | 1 | 1 | STATUS
// | 1 | 0 | 0 | RESERVED
// | 1 | 0 | 1 | RESERVED
// | 1 | 1 | 0 | MESSAGE OUT
// | 1 | 1 | 1 | MESSAGE IN
const array<phase_t, 8> BUS::phase_table = {
    phase_t::dataout,
    phase_t::datain,
    phase_t::command,
    phase_t::status,
    phase_t::reserved,
    phase_t::reserved,
    phase_t::msgout,
    phase_t::msgin
};

//---------------------------------------------------------------------------
//
// Phase string to phase mapping
//
//---------------------------------------------------------------------------
const unordered_map<phase_t, const char*> BUS::phase_str_mapping = {
	{ phase_t::busfree, "busfree" },
	{ phase_t::arbitration, "arbitration" },
	{ phase_t::selection, "selection" },
	{ phase_t::reselection, "reselection" },
	{ phase_t::command, "command" },
	{ phase_t::datain, "datain" },
	{ phase_t::dataout, "dataout" },
	{ phase_t::status, "status" },
	{ phase_t::msgin, "msgin" },
	{ phase_t::msgout, "msgout" },
	{ phase_t::reserved, "reserved" }
};
