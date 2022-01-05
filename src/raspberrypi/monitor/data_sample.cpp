//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*) for Raspberry Pi
//
//	Copyright (C) 2020-2021 akuker
//
//	[ SCSI Bus Monitor ]
//
//---------------------------------------------------------------------------
#include "os.h"
#include "scsi.h"
#include "data_sample.h"

const char *GetPhaseStr(const data_capture *sample)
{
	return BUS::GetPhaseStrRaw(GetPhase(sample));
}

BUS::phase_t GetPhase(const data_capture *sample)
{
	// Selection Phase
	if (GetSel(sample))
	{
		return BUS::selection;
	}

	// Bus busy phase
	if (!GetBsy(sample))
	{
		return BUS::busfree;
	}

	// Get target phase from bus signal line
	DWORD mci = GetMsg(sample) ? 0x04 : 0x00;
	mci |= GetCd(sample) ? 0x02 : 0x00;
	mci |= GetIo(sample) ? 0x01 : 0x00;
	return BUS::GetPhase(mci);
}
