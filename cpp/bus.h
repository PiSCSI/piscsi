//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//---------------------------------------------------------------------------

#pragma once

#include "os.h"
#include "scsi.h"
#include <array>
#include <unordered_map>
#include "hal/board_type.h"

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

	// Basic Functions
	virtual bool Init(mode_e mode, board_type::rascsi_board_type_e rascsi_type ) = 0;

	virtual void Reset() = 0;
	virtual void Cleanup() = 0;
	phase_t GetPhase();

	static phase_t GetPhase(int mci)
	{
		return phase_table[mci];
	}

	// Get the string phase name, based upon the raw data
	static const char* GetPhaseStrRaw(phase_t current_phase);

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

	virtual BYTE GetDAT() = 0;
	virtual void SetDAT(BYTE dat) = 0;
	virtual bool GetDP() const = 0;			// Get parity signal

	virtual uint32_t Acquire() = 0;
	virtual int CommandHandShake(BYTE *buf) = 0;
	virtual int ReceiveHandShake(BYTE *buf, int count) = 0;
	virtual int SendHandShake(BYTE *buf, int count, int delay_after_bytes) = 0;

	virtual bool GetSignal(board_type::pi_physical_pin_e pin) const = 0;
										// Get SCSI input signal value
	virtual void SetSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) = 0;
										// Set SCSI output signal value
	static const int SEND_NO_DELAY = -1;
										// Passed into SendHandShake when we don't want to delay
private:
	static const array<phase_t, 8> phase_table;

	static const unordered_map<phase_t, const char *> phase_str_mapping;
};
