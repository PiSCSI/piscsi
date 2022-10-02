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
#include "phase_handler.h"
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <memory>

using namespace std;

class PrimaryDevice;

class AbstractController : public PhaseHandler
{
	friend class PrimaryDevice;
	friend class ScsiController;

	// Logical units of this controller mapped to their LUN numbers
	unordered_map<int, shared_ptr<PrimaryDevice>> luns;

public:

	enum class rascsi_shutdown_mode {
		NONE,
		STOP_RASCSI,
		STOP_PI,
		RESTART_PI
	};

	using ctrl_t = struct _ctrl_t {
		vector<int> cmd;				// Command data, dynamically allocated per received command
		scsi_defs::status status;		// Status data
		int message;					// Message data

		// Transfer
		vector<BYTE> buffer;			// Transfer data buffer
		uint32_t blocks;				// Number of transfer blocks
		uint64_t next;					// Next record
		uint32_t offset;				// Transfer offset
		uint32_t length;				// Transfer remaining length
	};

	AbstractController(BUS& bus, int target_id, int max_luns) : target_id(target_id), bus(bus), max_luns(max_luns) {}
	~AbstractController() override = default;

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
	int GetLunCount() const { return (int)luns.size(); }

	unordered_set<shared_ptr<PrimaryDevice>> GetDevices() const;
	shared_ptr<PrimaryDevice> GetDeviceForLun(int) const;
	bool AddDevice(shared_ptr<PrimaryDevice>);
	bool DeleteDevice(const shared_ptr<PrimaryDevice>);
	bool HasDeviceForLun(int) const;
	int ExtractInitiatorId(int) const;

	void AllocateBuffer(size_t);
	vector<BYTE>& GetBuffer() { return ctrl.buffer; }
	scsi_defs::status GetStatus() const { return ctrl.status; }
	void SetStatus(scsi_defs::status s) { ctrl.status = s; }
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

private:

	int target_id;

	BUS& bus;

	int max_luns;

	ctrl_t ctrl = {};

	ctrl_t* GetCtrl() { return &ctrl; }
};
