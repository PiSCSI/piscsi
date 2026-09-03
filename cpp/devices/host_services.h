//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// Host Services with realtime clock and shutdown support
//
//---------------------------------------------------------------------------

#pragma once

#include "mode_page_device.h"
#include <span>
#include <vector>
#include <map>
#include <functional>
#include <utility>

class HostServices: public ModePageDevice
{

public:

	explicit HostServices(int lun) : ModePageDevice(SCHS, lun) {}
	~HostServices() override = default;

	bool Init(const param_map&) override;

	vector<uint8_t> InquiryInternal() const override;
	void TestUnitReady() override;

	// Executes a remote-interface command without using its socket transport.
	using command_executor = function<bool(const PbCommand&, PbResult&)>;
	void SetCommandExecutor(command_executor executor) { execute_command = std::move(executor); }

	bool WriteByteSequence(span<const uint8_t>) override;

protected:

	void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;

private:

	using mode_page_datetime = struct __attribute__((packed)) {
		// Major and minor version of this data structure (e.g. 1.0)
	    uint8_t major_version;
	    uint8_t minor_version;
	    // Current date and time, with daylight savings time adjustment applied
	    uint8_t year; // year - 1900
	    uint8_t month; // 0-11
	    uint8_t day; // 1-31
	    uint8_t hour; // 0-23
	    uint8_t minute; // 0-59
	    uint8_t second; // 0-59
	};

	void StartStopUnit() const;
	void ExecuteOperation();
	void ReceiveOperationResults();
	int ModeSense6(cdb_t, vector<uint8_t>&) const override;
	int ModeSense10(cdb_t, vector<uint8_t>&) const override;

	void AddRealtimeClockPage(map<int, vector<byte>>&, bool) const;
	bool ParseCommand(span<const uint8_t>, PbCommand&) const;
	int GetTransferLength() const;

	enum class protobuf_format { binary = 0x01, json = 0x02, text = 0x04 };
	protobuf_format ConvertFormat() const;

	// Operation results are isolated by initiator ID.
	unordered_map<int, string> execution_results;
	command_executor execute_command;
	protobuf_format input_format = protobuf_format::binary;
};
