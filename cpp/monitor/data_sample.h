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

#include "scsi.h"
#include "hal/gpiobus.h"
#include "hal/board_type.h"

using data_capture_t = struct data_capture
{
	uint32_t data;
	uint64_t timestamp;
};

#define GET_PIN(SAMPLE, PIN) ((bool)((SAMPLE->data >> PIN) & 1))

extern shared_ptr<GPIOBUS> bus;			      // GPIO Bus

inline bool GetBsy(const data_capture *sample) { return bus->GetPinRaw(sample->data, bus->GetBoard()->pin_bsy); }
inline bool GetSel(const data_capture *sample) { return bus->GetPinRaw(sample->data, bus->GetBoard()->pin_sel); }
inline bool GetAtn(const data_capture *sample) { return bus->GetPinRaw(sample->data, bus->GetBoard()->pin_atn); }
inline bool GetAck(const data_capture *sample) { return bus->GetPinRaw(sample->data, bus->GetBoard()->pin_ack); }
inline bool GetRst(const data_capture *sample) { return bus->GetPinRaw(sample->data, bus->GetBoard()->pin_rst); }
inline bool GetMsg(const data_capture *sample) { return bus->GetPinRaw(sample->data, bus->GetBoard()->pin_msg); }
inline bool GetCd(const data_capture *sample)  { return bus->GetPinRaw(sample->data, bus->GetBoard()->pin_cd); }
inline bool GetIo(const data_capture *sample)  { return bus->GetPinRaw(sample->data, bus->GetBoard()->pin_io); }
inline bool GetReq(const data_capture *sample) { return bus->GetPinRaw(sample->data, bus->GetBoard()->pin_req); }
inline bool GetDp(const data_capture *sample)  { return bus->GetPinRaw(sample->data, bus->GetBoard()->pin_dp); }
inline uint8_t GetData(const data_capture *sample)
{
	uint32_t result = 0;
	result |= (bus->GetPinRaw(sample->data, bus->GetBoard()->pin_dt0) != 0) ? (1 << 0) : 0;
	result |= (bus->GetPinRaw(sample->data, bus->GetBoard()->pin_dt1) != 0) ? (1 << 1) : 0;
	result |= (bus->GetPinRaw(sample->data, bus->GetBoard()->pin_dt2) != 0) ? (1 << 2) : 0;
	result |= (bus->GetPinRaw(sample->data, bus->GetBoard()->pin_dt3) != 0) ? (1 << 3) : 0;
	result |= (bus->GetPinRaw(sample->data, bus->GetBoard()->pin_dt4) != 0) ? (1 << 4) : 0;
	result |= (bus->GetPinRaw(sample->data, bus->GetBoard()->pin_dt5) != 0) ? (1 << 5) : 0;
	result |= (bus->GetPinRaw(sample->data, bus->GetBoard()->pin_dt6) != 0) ? (1 << 6) : 0;
	result |= (bus->GetPinRaw(sample->data, bus->GetBoard()->pin_dt7) != 0) ? (1 << 7) : 0;
	return result;
}

const char *GetPhaseStr(const data_capture *sample);
BUS::phase_t GetPhase(const data_capture *sample);
