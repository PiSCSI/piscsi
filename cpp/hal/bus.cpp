//---------------------------------------------------------------------------
//
// X68000 EMULATOR "XM6"
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2022 Uwe Seimet
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
BUS::phase_t BUS::GetPhase()
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

//---------------------------------------------------------------------------
//
//	Phase Table
//    Reference Table 8: https://www.staff.uni-mainz.de/tacke/scsi/SCSI2-06.html
//  This determines the phase based upon the Msg, C/D and I/O signals.
//
//---------------------------------------------------------------------------
const array<BUS::phase_t, 8> BUS::phase_table = {
						// | MSG|C/D|I/O |
	phase_t::dataout,	// |  0 | 0 | 0  |
	phase_t::datain,	// |  0 | 0 | 1  |
	phase_t::command,	// |  0 | 1 | 0  |
	phase_t::status,	// |  0 | 1 | 1  |
	phase_t::reserved,	// |  1 | 0 | 0  |
	phase_t::reserved,	// |  1 | 0 | 1  |
	phase_t::msgout,	// |  1 | 1 | 0  |
	phase_t::msgin		// |  1 | 1 | 1  |
};

//---------------------------------------------------------------------------
//
// Phase string to phase mapping
//
//---------------------------------------------------------------------------
const unordered_map<BUS::phase_t, const char*> BUS::phase_str_mapping = {
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
