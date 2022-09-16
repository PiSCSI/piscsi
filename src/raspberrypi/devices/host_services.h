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

	vector<BYTE> InquiryInternal() const override;
	void TestUnitReady() override;
	void StartStopUnit();

	bool SupportsFile() const override { return false; }

protected:

	void AddModePages(map<int, vector<BYTE>>&, int, bool) const override;

private:

	using super = ModePageDevice;

	Dispatcher<HostServices> dispatcher;

	int ModeSense6(const DWORD *, BYTE *, int) override;
	int ModeSense10(const DWORD *, BYTE *, int) override;

	void AddRealtimeClockPage(map<int, vector<BYTE>>&, bool) const;

	const DeviceFactory *device_factory;
};
