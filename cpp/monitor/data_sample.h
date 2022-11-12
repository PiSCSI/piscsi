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

#pragma once

#include "shared/scsi.h"
#include "hal/gpiobus.h"

using data_capture_t = struct data_capture
{
	uint32_t data;
	uint64_t timestamp;
};

#define GET_PIN(SAMPLE, PIN) ((bool)((SAMPLE->data >> PIN) & 1))

bool TempGetPinRaw(uint32_t, int);

inline bool GetBsy(const data_capture *sample) { return TempGetPinRaw(sample->data, PIN_BSY); }
inline bool GetSel(const data_capture *sample) { return TempGetPinRaw(sample->data, PIN_SEL); }
inline bool GetAtn(const data_capture *sample) { return TempGetPinRaw(sample->data, PIN_ATN); }
inline bool GetAck(const data_capture *sample) { return TempGetPinRaw(sample->data, PIN_ACK); }
inline bool GetRst(const data_capture *sample) { return TempGetPinRaw(sample->data, PIN_RST); }
inline bool GetMsg(const data_capture *sample) { return TempGetPinRaw(sample->data, PIN_MSG); }
inline bool GetCd(const data_capture *sample)  { return TempGetPinRaw(sample->data, PIN_CD); }
inline bool GetIo(const data_capture *sample)  { return TempGetPinRaw(sample->data, PIN_IO); }
inline bool GetReq(const data_capture *sample) { return TempGetPinRaw(sample->data, PIN_REQ); }
inline bool GetDp(const data_capture *sample)  { return TempGetPinRaw(sample->data, PIN_DP); }
inline uint8_t GetData(const data_capture *sample)
{
	uint32_t data = sample->data;
	return (uint8_t)((data >> (PIN_DT0 - 0)) & (1 << 0)) |
		   ((data >> (PIN_DT1 - 1)) & (1 << 1)) |
		   ((data >> (PIN_DT2 - 2)) & (1 << 2)) |
		   ((data >> (PIN_DT3 - 3)) & (1 << 3)) |
		   ((data >> (PIN_DT4 - 4)) & (1 << 4)) |
		   ((data >> (PIN_DT5 - 5)) & (1 << 5)) |
		   ((data >> (PIN_DT6 - 6)) & (1 << 6)) |
		   ((data >> (PIN_DT7 - 7)) & (1 << 7));
}

const char *GetPhaseStr(const data_capture *sample);
BUS::phase_t GetPhase(const data_capture *sample);
