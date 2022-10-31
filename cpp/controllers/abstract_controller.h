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
#include "bus.h"
#include "phase_handler.h"
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <memory>

using namespace std;

class PrimaryDevice;

class AbstractController : public PhaseHandler
{
	friend class ScsiController;

public:

	enum class rascsi_shutdown_mode {
		NONE,
		STOP_RASCSI,
		STOP_PI,
		RESTART_PI
	};

	AbstractController(shared_ptr<BUS> bus, int target_id, int max_luns) : target_id(target_id), bus(bus), max_luns(max_luns) {}
	~AbstractController() override = default;

	virtual void Error(scsi_defs::sense_key, scsi_defs::asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status = scsi_defs::status::CHECK_CONDITION) = 0;
	virtual void Reset();
	virtual int GetInitiatorId() const = 0;

	// Get requested LUN based on IDENTIFY message, with LUN from the CDB as fallback
	virtual int GetEffectiveLun() const = 0;

	virtual void ScheduleShutdown(rascsi_shutdown_mode) = 0;

	int GetTargetId() const { return target_id; }
	int GetMaxLuns() const { return max_luns; }
	int GetLunCount() const { return static_cast<int>(luns.size()); }

	unordered_set<shared_ptr<PrimaryDevice>> GetDevices() const;
	shared_ptr<PrimaryDevice> GetDeviceForLun(int) const;
	bool AddDevice(shared_ptr<PrimaryDevice>);
	bool RemoveDevice(const shared_ptr<PrimaryDevice>);
	bool HasDeviceForLun(int) const;
	int ExtractInitiatorId(int) const;

	void AllocateBuffer(size_t);
	vector<BYTE>& GetBuffer() { return ctrl.buffer; }
	scsi_defs::status GetStatus() const { return ctrl.status; }
	void SetStatus(scsi_defs::status s) { ctrl.status = s; }
	uint32_t GetLength() const { return ctrl.length; }
	void SetLength(uint32_t l) { ctrl.length = l; }
	uint32_t GetBlocks() const { return ctrl.blocks; }
	void SetBlocks(uint32_t b) { ctrl.blocks = b; }
	void DecrementBlocks() { --ctrl.blocks; }
	void SetNext(uint64_t n) { ctrl.next = n; }

	vector<int>& GetCmd() { return ctrl.cmd; }
	int GetCmd(int index) const { return ctrl.cmd[index]; }

	bool IsByteTransfer() const { return is_byte_transfer; }
	void SetByteTransfer(bool);

protected:

	scsi_defs::scsi_command GetOpcode() const { return static_cast<scsi_defs::scsi_command>(ctrl.cmd[0]); }
	int GetLun() const { return (ctrl.cmd[1] >> 5) & 0x07; }

	void ProcessPhase();

	void AllocateCmd(size_t);

	bool HasValidLength() const { return ctrl.length != 0; }
	int GetOffset() const { return ctrl.offset; }
	void ResetOffset() { ctrl.offset = 0; }
	void UpdateOffsetAndLength() { ctrl.offset += ctrl.length; ctrl.length = 0; }

private:

	using ctrl_t = struct _ctrl_t {
		// Command data, dynamically resized if required
		vector<int> cmd = vector<int>(16);

		scsi_defs::status status;		// Status data
		int message;					// Message data

		// Transfer
		vector<BYTE> buffer;			// Transfer data buffer
		uint32_t blocks;				// Number of transfer blocks
		uint64_t next;					// Next record
		uint32_t offset;				// Transfer offset
		uint32_t length;				// Transfer remaining length
	};

	ctrl_t ctrl = {};

	// Logical units of this controller mapped to their LUN numbers
	unordered_map<int, shared_ptr<PrimaryDevice>> luns;

	int target_id;

	shared_ptr<BUS> bus;

	int max_luns;

	bool is_byte_transfer = false;
	uint32_t bytes_to_transfer = 0;
};
