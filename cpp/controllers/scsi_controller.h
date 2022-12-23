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
#include "controller_manager.h"
#include "devices/device_logger.h"
#include "abstract_controller.h"
#include <array>

using namespace std;

class PrimaryDevice;

class ScsiController : public AbstractController
{
	// For timing adjustments
	static const unsigned int MIN_EXEC_TIME = 50;

	// Transfer period factor (limited to 50 x 4 = 200ns)
	static const int MAX_SYNC_PERIOD = 50;

	// REQ/ACK offset(limited to 16)
	static const uint8_t MAX_SYNC_OFFSET = 16;

	const int DEFAULT_BUFFER_SIZE = 0x1000;

	using scsi_t = struct _scsi_t {
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

	// Maximum number of logical units
	static inline const int LUN_MAX = 32;

	explicit ScsiController(shared_ptr<ControllerManager>, int);
	~ScsiController() override = default;

	void Reset() override;

	phase_t Process(int) override;

	int GetEffectiveLun() const override;

	void Error(scsi_defs::sense_key sense_key, scsi_defs::asc asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status status = scsi_defs::status::CHECK_CONDITION) override;

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

	// TODO Make non-virtual private as soon as SysTimer calls do not segfault anymore on a regular PC,
	// e.g. by using ifdef __arm__. Currently the unit tests require this method to be public.
	virtual void Execute();

	void ScheduleShutdown(piscsi_shutdown_mode mode) override { shutdown_mode = mode; }

private:

	DeviceLogger logger;

	// Execution start time
	uint32_t execstart = 0;

	// The initiator ID may be unavailable, e.g. with Atari ACSI and old host adapters
	int initiator_id = UNKNOWN_INITIATOR_ID;

	// The LUN from the IDENTIFY message
	int identified_lun = -1;

	// Data transfer
	void Send();
	bool XferMsg(int);
	bool XferIn(vector<uint8_t>&);
	bool XferOut(bool);
	bool XferOutBlockOriented(bool);
	void ReceiveBytes();

	void DataOutNonBlockOriented();
	void Receive();

	void ProcessCommand();
	void ParseMessage();
	void ProcessMessage();

	void Sleep();

	scsi_t scsi = {};

	AbstractController::piscsi_shutdown_mode shutdown_mode = AbstractController::piscsi_shutdown_mode::NONE;
};

