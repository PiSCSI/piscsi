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
BUS::phase_t FASTCALL BUS::GetPhase()
{
	DWORD mci;

	ASSERT(this);

	// Selection Phase
	if (GetSEL()) {
		return selection;
	}

	// Bus busy phase
	if (!GetBSY()) {
		return busfree;
	}

	// Get target phase from bus signal line
	mci = GetMSG() ? 0x04 : 0x00;
	mci |= GetCD() ? 0x02 : 0x00;
	mci |= GetIO() ? 0x01 : 0x00;
	return GetPhase(mci);
}

//---------------------------------------------------------------------------
//
//	Phase Table
//
//---------------------------------------------------------------------------
const BUS::phase_t BUS::phase_table[8] = {
	dataout,
	datain,
	command,
	status,
	reserved,
	reserved,
	msgout,
	msgin
};
