//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Host Services with realtime clock and shutdown support
//
//---------------------------------------------------------------------------
#pragma once

#include "mode_page_device.h"
#include <vector>
#include <map>

using namespace std;

class DeviceFactory;

class HostServices: public ModePageDevice
{

public:

	explicit HostServices(const DeviceFactory *);
	~HostServices() override = default;
	HostServices(HostServices&) = delete;
	HostServices& operator=(const HostServices&) = delete;

	bool Dispatch() override;

	vector<byte> InquiryInternal() const override;
	void TestUnitReady() override;
	void StartStopUnit();

	bool SupportsFile() const override { return false; }

protected:

	void AddModePages(map<int, vector<byte>>&, int, bool) const override;

private:

	using mode_page_datetime = struct __attribute__((packed)) {
		// Major and minor version of this data structure (1.0)
	    uint8_t major_version = 0x01;
	    uint8_t minor_version = 0x00;
	    // Current date and time, with daylight savings time adjustment applied
	    uint8_t year; // year - 1900
	    uint8_t month; // 0-11
	    uint8_t day; // 1-31
	    uint8_t hour; // 0-23
	    uint8_t minute; // 0-59
	    uint8_t second; // 0-59
	};

	using super = ModePageDevice;

	Dispatcher<HostServices> dispatcher;

	int ModeSense6(const DWORD *, BYTE *, int) const override;
	int ModeSense10(const DWORD *, BYTE *, int) const override;

	void AddRealtimeClockPage(map<int, vector<byte>>&, bool) const;

	const DeviceFactory *device_factory;
};
