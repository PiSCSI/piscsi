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

class HostServices: public ModePageDevice
{

public:

	explicit HostServices(int lun) : ModePageDevice(SCHS, lun) {}
	~HostServices() override = default;

	bool Init(const unordered_map<string, string>&) override;

	vector<uint8_t> InquiryInternal() const override;
	void TestUnitReady() override;

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
	int ModeSense6(cdb_t, vector<uint8_t>&) const override;
	int ModeSense10(cdb_t, vector<uint8_t>&) const override;

	void AddRealtimeClockPage(map<int, vector<byte>>&, bool) const;
};
