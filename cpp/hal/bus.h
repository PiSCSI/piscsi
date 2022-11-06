//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi.h"
#include <cstdint>
#include <array>
#include <vector>
#include <unordered_map>

using namespace std;

class BUS
{
public:
	// Operation modes definition
	enum class mode_e {
		TARGET = 0,
		INITIATOR = 1,
		MONITOR = 2,
	};

	//	Phase definitions
	enum class phase_t : int {
		busfree,
		arbitration,
		selection,
		reselection,
		command,
		datain,
		dataout,
		status,
		msgin,
		msgout,
		reserved
	};

	BUS() = default;
	virtual ~BUS() = default;

    static int GetCommandByteCount(uint8_t);

	virtual bool Init(mode_e mode) = 0;
	virtual void Reset() = 0;
	virtual void Cleanup() = 0;
	phase_t GetPhase();

	static phase_t GetPhase(int mci)
	{
		return phase_table[mci];
	}

	// Get the string phase name, based upon the raw data
	static const char* GetPhaseStrRaw(phase_t current_phase);

	// Extract as specific pin field from a raw data capture
	static inline uint32_t GetPinRaw(uint32_t raw_data, uint32_t pin_num)
	{
		return ((raw_data >> pin_num) & 1);
	}

	virtual bool GetBSY() const = 0;
	virtual void SetBSY(bool ast) = 0;

	virtual bool GetSEL() const = 0;
	virtual void SetSEL(bool ast) = 0;

	virtual bool GetATN() const = 0;
	virtual void SetATN(bool ast) = 0;

	virtual bool GetACK() const = 0;
	virtual void SetACK(bool ast) = 0;

	virtual bool GetRST() const = 0;
	virtual void SetRST(bool ast) = 0;

	virtual bool GetMSG() const = 0;
	virtual void SetMSG(bool ast) = 0;

	virtual bool GetCD() const = 0;
	virtual void SetCD(bool ast) = 0;

	virtual bool GetIO() = 0;
	virtual void SetIO(bool ast) = 0;

	virtual bool GetREQ() const = 0;
	virtual void SetREQ(bool ast) = 0;

	virtual uint8_t GetDAT() = 0;
	virtual void SetDAT(uint8_t dat) = 0;
	virtual bool GetDP() const = 0;			// Get parity signal

	virtual uint32_t Acquire() = 0;
	virtual int CommandHandShake(vector<uint8_t>&) = 0;
	virtual int ReceiveHandShake(uint8_t *buf, int count) = 0;
	virtual int SendHandShake(uint8_t *buf, int count, int delay_after_bytes) = 0;

	virtual bool GetSignal(int pin) const = 0;
										// Get SCSI input signal value
	virtual void SetSignal(int pin, bool ast) = 0;
										// Set SCSI output signal value
	static const int SEND_NO_DELAY = -1;
										// Passed into SendHandShake when we don't want to delay
private:
	static const array<phase_t, 8> phase_table;

	static const unordered_map<phase_t, const char *> phase_str_mapping;
};
