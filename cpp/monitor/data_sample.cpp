//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2020-2021 akuker
//
//	[ SCSI Bus Monitor ]
//
//---------------------------------------------------------------------------

#include "shared/scsi.h"
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
		return BUS::phase_t::selection;
	}

	// Bus busy phase
	if (!GetBsy(sample))
	{
		return BUS::phase_t::busfree;
	}

	// Get target phase from bus signal line
	uint32_t mci = GetMsg(sample) ? 0x04 : 0x00;
	mci |= GetCd(sample) ? 0x02 : 0x00;
	mci |= GetIo(sample) ? 0x01 : 0x00;
	return BUS::GetPhase(mci);
}

// TODO: This will only work for Raspberry Pi! 
bool TempGetPinRaw(uint32_t data, int gpio){
	return ((data & (1 << gpio)) != 0);
}
