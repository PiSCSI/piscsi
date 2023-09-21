//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// Base class for device controllers
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/scsi.h"
#include "hal/bus.h"
#include "phase_handler.h"
#include "controller_manager.h"
#include "devices/device_logger.h"
#include <unordered_set>
#include <unordered_map>
#include <span>
#include <vector>
#include <memory>
#include <functional>

using namespace std;

class PrimaryDevice;

class AbstractController : public PhaseHandler, public enable_shared_from_this<AbstractController>
{
public:

	static inline const int UNKNOWN_INITIATOR_ID = -1;

	enum class piscsi_shutdown_mode {
		NONE,
		STOP_PISCSI,
		STOP_PI,
		RESTART_PI
	};

	AbstractController(ControllerManager& controller_manager, int target_id, int max_luns)
		: controller_manager(controller_manager), target_id(target_id), max_luns(max_luns) {}
	~AbstractController() override = default;

	virtual void Error(scsi_defs::sense_key, scsi_defs::asc = scsi_defs::asc::no_additional_sense_information,
			scsi_defs::status = scsi_defs::status::check_condition) = 0;
	virtual void Reset();
	virtual int GetInitiatorId() const = 0;

	// Get requested LUN based on IDENTIFY message, with LUN from the CDB as fallback
	virtual int GetEffectiveLun() const = 0;

	virtual void ScheduleShutdown(piscsi_shutdown_mode) = 0;

	int GetTargetId() const { return target_id; }
	int GetMaxLuns() const { return max_luns; }
	int GetLunCount() const { return static_cast<int>(luns.size()); }

	unordered_set<shared_ptr<PrimaryDevice>> GetDevices() const;
	shared_ptr<PrimaryDevice> GetDeviceForLun(int) const;
	bool AddDevice(shared_ptr<PrimaryDevice>);
	bool RemoveDevice(shared_ptr<PrimaryDevice>);
	bool HasDeviceForLun(int) const;
	void ProcessOnController(int);

	// TODO These should probably be extracted into a new TransferHandler class
	void AllocateBuffer(size_t);
	auto& GetBuffer() { return ctrl.buffer; }
	scsi_defs::status GetStatus() const { return ctrl.status; }
	void SetStatus(scsi_defs::status s) { ctrl.status = s; }
	uint32_t GetLength() const { return ctrl.length; }
	void SetLength(uint32_t l) { ctrl.length = l; }
	uint32_t GetBlocks() const { return ctrl.blocks; }
	void SetBlocks(uint32_t b) { ctrl.blocks = b; }
	void DecrementBlocks() { --ctrl.blocks; }
	uint64_t GetNext() const { return ctrl.next; }
	void SetNext(uint64_t n) { ctrl.next = n; }
	void IncrementNext() { ++ctrl.next; }
	int GetMessage() const { return ctrl.message; }
	void SetMessage(int m) { ctrl.message = m; }
	cdb_t GetCmd() const { return ctrl.cmd; }
	int GetCmdByte(int index) const { return ctrl.cmd[index]; }
	bool IsByteTransfer() const { return is_byte_transfer; }
	void SetByteTransfer(bool);
	uint32_t GetBytesToTransfer() const { return bytes_to_transfer; }
	void SetBytesToTransfer(uint32_t b) { bytes_to_transfer = b; }

protected:

	auto GetControllerManager() const { return controller_manager; }
	BUS& GetBus() const { return controller_manager.GetBus(); }

	auto GetOpcode() const { return static_cast<scsi_defs::scsi_command>(ctrl.cmd[0]); }
	int GetLun() const { return (ctrl.cmd[1] >> 5) & 0x07; }

	void AllocateCmd(size_t);

	void SetCmdByte(int index, int value) { ctrl.cmd[index] = value; }

	// TODO These should probably be extracted into a new TransferHandler class
	bool HasValidLength() const { return ctrl.length != 0; }
	int GetOffset() const { return ctrl.offset; }
	void ResetOffset() { ctrl.offset = 0; }
	void UpdateOffsetAndLength() { ctrl.offset += ctrl.length; ctrl.length = 0; }

private:

	int ExtractInitiatorId(int) const;

	using ctrl_t = struct _ctrl_t {
		// Command data, dynamically resized if required
		vector<int> cmd = vector<int>(16);

		scsi_defs::status status;		// Status data
		int message;					// Message data

		// Transfer
		vector<uint8_t> buffer;			// Transfer data buffer
		uint32_t blocks;				// Number of transfer blocks
		uint64_t next;					// Next record
		uint32_t offset;				// Transfer offset
		uint32_t length;				// Transfer remaining length
	};

	ctrl_t ctrl = {};

	ControllerManager& controller_manager;

	// Logical units of this controller mapped to their LUN numbers
	unordered_map<int, shared_ptr<PrimaryDevice>> luns;

	int target_id;

	int max_luns;

	bool is_byte_transfer = false;
	uint32_t bytes_to_transfer = 0;

	DeviceLogger device_logger;
};
