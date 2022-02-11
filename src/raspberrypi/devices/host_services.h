//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// RaSCSI Host Services with realtime clock and shutdown support
//
//---------------------------------------------------------------------------
#pragma once

#include "mode_page_device.h"
#include <map>

using namespace std;

class HostServices: public ModePageDevice
{
public:

	HostServices();
	~HostServices() {}

	virtual bool Dispatch(SCSIDEV *) override;

	int Inquiry(const DWORD *, BYTE *) override;
	void TestUnitReady(SASIDEV *) override;
	void StartStopUnit(SASIDEV *);

	int ModeSense6(const DWORD *, BYTE *);
	int ModeSense10(const DWORD *, BYTE *);

private:

	Dispatcher<HostServices> dispatcher;

	int AddRealtimeClockPage(int, BYTE *);
};
