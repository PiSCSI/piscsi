//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License.
//	See LICENSE file in the project root folder.
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/scsi.h"
#include "abstract_controller.h"
#include <array>

using namespace std;

class PrimaryDevice;

// Mac Plus/SCSI-1 compatibility mode
// Mac Plus and other early SCSI-1 hosts have timing and protocol quirks
enum class CompatibilityMode {
	STANDARD,     // Standard SCSI-2 operation
	MAC_PLUS,     // Mac Plus compatibility (SCSI-1 with specific quirks)
	SCSI1_COMPAT  // Generic SCSI-1 compatibility
};

class ScsiController : public AbstractController
{
	// For timing adjustments
public:
	static unsigned int MIN_EXEC_TIME;
	static void SetMinExecTime(unsigned int usec) { MIN_EXEC_TIME = usec; }

	// Mac Plus/SCSI-1 compatibility mode
	static CompatibilityMode default_compat_mode;
	static void SetDefaultCompatibilityMode(CompatibilityMode mode) { default_compat_mode = mode; }
	static CompatibilityMode GetDefaultCompatibilityMode() { return default_compat_mode; }

	// Transfer period factor (limited to 50 x 4 = 200ns)
	static const int MAX_SYNC_PERIOD = 50;

	// REQ/ACK offset(limited to 16)
	static const uint8_t MAX_SYNC_OFFSET = 16;

	const int DEFAULT_BUFFER_SIZE = 0x1000;

	struct scsi_t {
		// Synchronous transfer
		bool syncenable;				// Synchronous transfer possible
		uint8_t syncperiod = MAX_SYNC_PERIOD;	// Synchronous transfer period
		uint8_t syncoffset;					// Synchronous transfer offset
		int syncack;					// Number of synchronous transfer ACKs

		// ATN message
		bool atnmsg;
		int msc;
		array<uint8_t, 256> msb;
	};

public:

	ScsiController(BUS&, int);
	~ScsiController() override = default;

	void Reset() override;

	bool Process(int) override;

	int GetEffectiveLun() const override;

	void Error(scsi_defs::sense_key sense_key, scsi_defs::asc asc = scsi_defs::asc::no_additional_sense_information,
			scsi_defs::status status = scsi_defs::status::check_condition) override;

	int GetInitiatorId() const override { return initiator_id; }

	// Phases
	void BusFree() override;
	void Selection() override;
	void Command() override;
	void MsgIn() override;
	void MsgOut() override;
	void Status() override;
	void DataIn() override;
	void DataOut() override;

private:

	// Execution start time
	uint32_t execstart = 0;

	// The initiator ID may be unavailable, e.g. with Atari ACSI and old host adapters
	int initiator_id = UNKNOWN_INITIATOR_ID;

	// The LUN from the IDENTIFY message
	int identified_lun = -1;

	// SCSI-1 host detected (no ATN during selection - e.g., Mac Plus)
	bool scsi1_detected = false;

	// Instance-level compatibility mode (initialized from static default)
	CompatibilityMode compat_mode = CompatibilityMode::STANDARD;

	// Enable SCSI-1 compatibility behaviors on all attached devices
	void EnableScsi1Compatibility();

	// Check if running in Mac Plus or SCSI-1 compatibility mode
	bool IsMacPlusMode() const { return compat_mode == CompatibilityMode::MAC_PLUS; }
	bool IsScsi1Mode() const { return scsi1_detected || compat_mode != CompatibilityMode::STANDARD; }

	// Data transfer
	void Send();
	bool XferMsg(int);
	bool XferIn(vector<uint8_t>&);
	bool XferOut(bool);
	bool XferOutBlockOriented(bool);
	void ReceiveBytes();

	void DataOutNonBlockOriented() const;
	void Receive();

	// TODO Make non-virtual as soon as SysTimer calls do not segfault anymore on a regular PC, e.g. by using ifdef __arm__.
	virtual void Execute();

	void ProcessCommand();
	void ParseMessage();
	void ProcessMessage();

	void Sleep();

	scsi_t scsi = {};
};

