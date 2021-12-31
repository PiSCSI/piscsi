//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ GPIO-SCSI bus ]
//
//---------------------------------------------------------------------------

#pragma once

#include "os.h"
#include "rascsi.h"
#include "scsi.h"
#include "gpiobus.h"

typedef struct data_capture{
    DWORD data;
    uint64_t timestamp;
} data_capture_t;

#define GET_PIN(SAMPLE,PIN) ((bool)((SAMPLE->data >> PIN) & 1))

inline bool GetBsy(data_capture *sample) { return BUS::GetPinRaw(sample->data, PIN_BSY);}
inline bool GetSel(data_capture *sample) { return BUS::GetPinRaw(sample->data, PIN_SEL);}
inline bool GetAtn(data_capture *sample) { return BUS::GetPinRaw(sample->data, PIN_ATN);}
inline bool GetAck(data_capture *sample) { return BUS::GetPinRaw(sample->data, PIN_ACK);}
inline bool GetRst(data_capture *sample) { return BUS::GetPinRaw(sample->data, PIN_RST);}
inline bool GetMsg(data_capture *sample) { return BUS::GetPinRaw(sample->data, PIN_MSG);}
inline bool GetCd(data_capture *sample)  { return BUS::GetPinRaw(sample->data, PIN_CD);}
inline bool GetIo(data_capture *sample)  { return BUS::GetPinRaw(sample->data, PIN_IO);}
inline bool GetReq(data_capture *sample) { return BUS::GetPinRaw(sample->data, PIN_REQ);}
inline bool GetDp(data_capture *sample)  { return BUS::GetPinRaw(sample->data, PIN_DP);}
inline BYTE GetData(data_capture *sample){
    DWORD data = sample->data;
	return (BYTE)
		((data >> (PIN_DT0 - 0)) & (1 << 0)) |
		((data >> (PIN_DT1 - 1)) & (1 << 1)) |
		((data >> (PIN_DT2 - 2)) & (1 << 2)) |
		((data >> (PIN_DT3 - 3)) & (1 << 3)) |
		((data >> (PIN_DT4 - 4)) & (1 << 4)) |
		((data >> (PIN_DT5 - 5)) & (1 << 5)) |
		((data >> (PIN_DT6 - 6)) & (1 << 6)) |
		((data >> (PIN_DT7 - 7)) & (1 << 7));
}

const char* GetPhaseStr(data_capture *sample);
BUS::phase_t GetPhase(data_capture *sample);



// class GPIOSim : BUS
// {

// 	GPIOSim();
// 	~GPIOSim() override;

// 	// Basic Functions
// 	BOOL Init(mode_e mode) override;
// 	void Reset() override {};
// 	void Cleanup() override {};

// 	bool GetBSY() override;
// 	void SetBSY(bool ast) override {};

// 	BOOL GetSEL() override;
// 	void SetSEL(BOOL ast) override {};

// 	BOOL GetATN() override;
// 	void SetATN(BOOL ast) override {};

// 	BOOL GetACK() override;
// 	void SetACK(BOOL ast) override {};

// 	BOOL GetRST() override;
// 	void SetRST(BOOL ast) override {};

// 	BOOL GetMSG() override;
// 	void SetMSG(BOOL ast) override {};

// 	BOOL GetCD() override;
// 	void SetCD(BOOL ast) override {};

// 	BOOL GetIO() override;
// 	void SetIO(BOOL ast) override {};

// 	BOOL GetREQ() override;
// 	void SetREQ(BOOL ast) override {};

// 	BYTE GetDAT() override;
// 	void SetDAT(BYTE dat) override {};
// 	BOOL GetDP() override;			// Get parity signal

// 	int CommandHandShake(BYTE *buf) override;
// 	int ReceiveHandShake(BYTE *buf, int count) override;
// 	int SendHandShake(BYTE *buf, int count, int delay_after_bytes) override;

// 	BOOL GetSignal(int pin) override;
// 								// Get SCSI input signal value
// 	void SetSignal(int pin, BOOL ast) override;
// 								// Set SCSI output signal value

//     private:
//         DWORD Aquire();

//         DWORD current_gpio = 0;
// };