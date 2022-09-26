//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Base class for device controllers
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi.h"
#include <unordered_map>
#include <vector>
#include <memory>

using namespace std; //NOSONAR Not relevant for rascsi

class PrimaryDevice;

class AbstractController
{
	friend class PrimaryDevice;
	friend class ScsiController;

	BUS::phase_t phase = BUS::phase_t::busfree;

	// Logical units of this device controller mapped to their LUN numbers
	unordered_map<int, PrimaryDevice *> luns;

public:

	enum class rascsi_shutdown_mode {
		NONE,
		STOP_RASCSI,
		STOP_PI,
		RESTART_PI
	};

	using ctrl_t = struct _ctrl_t {
		vector<int> cmd;				// Command data, dynamically allocated per received command
		uint32_t status;				// Status data
		int message;					// Message data

		// Transfer
		vector<BYTE> buffer;			// Transfer data buffer
		uint32_t blocks;				// Number of transfer blocks
		uint64_t next;					// Next record
		uint32_t offset;				// Transfer offset
		uint32_t length;				// Transfer remaining length
	};

	AbstractController(shared_ptr<BUS> bus, int target_id, int max_luns) :
		target_id(target_id), bus(bus), max_luns(max_luns) {}
	virtual ~AbstractController() = default;
	AbstractController(AbstractController&) = delete;
	AbstractController& operator=(const AbstractController&) = delete;

	virtual void BusFree() = 0;
	virtual void Selection() = 0;
	virtual void Command() = 0;
	virtual void Status() = 0;
	virtual void DataIn() = 0;
	virtual void DataOut() = 0;
	virtual void MsgIn() = 0;
	virtual void MsgOut() = 0;

	virtual BUS::phase_t Process(int) = 0;

	virtual void Error(scsi_defs::sense_key, scsi_defs::asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status = scsi_defs::status::CHECK_CONDITION) = 0;
	virtual void Reset();
	virtual int GetInitiatorId() const = 0;
	virtual void SetByteTransfer(bool) = 0;

	// Get requested LUN based on IDENTIFY message, with LUN from the CDB as fallback
	virtual int GetEffectiveLun() const = 0;

	virtual void ScheduleShutdown(rascsi_shutdown_mode) = 0;

	int GetTargetId() const { return target_id; }
	int GetMaxLuns() const { return max_luns; }
	bool HasLuns() const { return !luns.empty(); }

	PrimaryDevice *GetDeviceForLun(int) const;
	bool AddDevice(PrimaryDevice *);
	bool DeleteDevice(const PrimaryDevice *);
	bool HasDeviceForLun(int) const;
	int ExtractInitiatorId(int) const;

	void AllocateBuffer(size_t);
	vector<BYTE>& GetBuffer() { return ctrl.buffer; }
	uint32_t GetStatus() const { return ctrl.status; }
	void SetStatus(uint32_t s) { ctrl.status = s; }
	uint32_t GetLength() const { return ctrl.length; }

protected:

	scsi_defs::scsi_command GetOpcode() const { return (scsi_defs::scsi_command)ctrl.cmd[0]; }
	int GetLun() const { return (ctrl.cmd[1] >> 5) & 0x07; }

	void ProcessPhase();

	vector<int>& InitCmd(int size) { ctrl.cmd.resize(size); return ctrl.cmd; }

	bool HasValidLength() const  { return ctrl.length != 0; }
	int GetOffset() const { return ctrl.offset; }
	void ResetOffset() { ctrl.offset = 0; }
	void UpdateOffsetAndLength() { ctrl.offset += ctrl.length; ctrl.length = 0; }

	BUS::phase_t GetPhase() const { return phase; }
	void SetPhase(BUS::phase_t p) { phase = p; }
	bool IsSelection() const { return phase == BUS::phase_t::selection; }
	bool IsBusFree() const { return phase == BUS::phase_t::busfree; }
	bool IsCommand() const { return phase == BUS::phase_t::command; }
	bool IsStatus() const { return phase == BUS::phase_t::status; }
	bool IsDataIn() const { return phase == BUS::phase_t::datain; }
	bool IsDataOut() const { return phase == BUS::phase_t::dataout; }
	bool IsMsgIn() const { return phase == BUS::phase_t::msgin; }
	bool IsMsgOut() const { return phase == BUS::phase_t::msgout; }

private:

	int target_id;

	shared_ptr<BUS> bus;

	int max_luns;

	ctrl_t ctrl = {};

	ctrl_t* GetCtrl() { return &ctrl; }
};
