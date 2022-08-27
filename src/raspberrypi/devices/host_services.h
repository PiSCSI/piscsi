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

using namespace std;

class ScsiController;

class HostServices: public ModePageDevice
{

public:

	HostServices();
	~HostServices() {}

	virtual bool Dispatch() override;

	vector<BYTE> InquiryInternal() const override;
	void TestUnitReady();
	void StartStopUnit();

	int ModeSense6(const DWORD *, BYTE *, int);
	int ModeSense10(const DWORD *, BYTE *, int);

	bool SupportsFile() const override { return false; }

private:

	typedef ModePageDevice super;

	Dispatcher<HostServices> dispatcher;

	void AddModePages(map<int, vector<BYTE>>&, int, bool) const override;
	void AddRealtimeClockPage(map<int, vector<BYTE>>&, bool) const;
};
