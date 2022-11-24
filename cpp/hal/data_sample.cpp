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

#include "hal/bus.h"
#include "hal/data_sample.h"
#include <string>

using namespace std;

const string DataSample::GetPhaseStr() const
{
    return BUS::GetPhaseStrRaw(GetPhase());
}

phase_t DataSample::GetPhase() const
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
    uint32_t mci = GetMSG() ? 0x04 : 0x00;
    mci |= GetCD() ? 0x02 : 0x00;
    mci |= GetIO() ? 0x01 : 0x00;
    return BUS::GetPhase(mci);
}
