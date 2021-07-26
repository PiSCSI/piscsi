//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	[ SCSI Common Functionality ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "scsi.h"

//---------------------------------------------------------------------------
//
//	Phase Acquisition
//
//---------------------------------------------------------------------------
BUS::phase_t BUS::GetPhase()
{
	// Selection Phase
	if (GetSEL()) {
		return selection;
	}

	// Bus busy phase
	if (!GetBSY()) {
		return busfree;
	}

	// Get target phase from bus signal line
	DWORD mci = GetMSG() ? 0x04 : 0x00;
	mci |= GetCD() ? 0x02 : 0x00;
	mci |= GetIO() ? 0x01 : 0x00;
	return GetPhase(mci);
}

//---------------------------------------------------------------------------
//
//	Determine Phase String phase enum
//
//---------------------------------------------------------------------------
const char* BUS::GetPhaseStrRaw(phase_t current_phase){
	if(current_phase <= phase_t::reserved){
		return phase_str_table[current_phase];
	}
	else
	{
		return "INVALID";
	}
}

//---------------------------------------------------------------------------
//
//	Phase Table
//    Reference Table 8: https://www.staff.uni-mainz.de/tacke/scsi/SCSI2-06.html
//  This determines the phase based upon the Msg, C/D and I/O signals.
//
//---------------------------------------------------------------------------
const BUS::phase_t BUS::phase_table[8] = {
	//        	   | MSG|C/D|I/O | 
	dataout,	// |  0 | 0 | 0  | 
	datain,		// |  0 | 0 | 1  | 
	command,	// |  0 | 1 | 0  | 
	status,		// |  0 | 1 | 1  | 
	reserved,	// |  1 | 0 | 0  | 
	reserved,	// |  1 | 0 | 1  | 
	msgout,		// |  1 | 1 | 0  | 
	msgin		// |  1 | 1 | 1  | 
};


//---------------------------------------------------------------------------
//
//	Phase Table
//      This MUST be kept in sync with the phase_t enum type!
//
//---------------------------------------------------------------------------
const char* BUS::phase_str_table[] = {
	"busfree",		
	"arbitration",	
	"selection",		
	"reselection",	
	"command",		
	"execute",		
	"datain",		
	"dataout",		
	"status",		
	"msgin",			
	"msgout",		
	"reserved"		
};
