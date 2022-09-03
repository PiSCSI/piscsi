//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "primary_device.h"
#include <string>
#include <vector>
#include <map>

using namespace std;

class ModePageDevice: public PrimaryDevice
{
public:

	ModePageDevice(const string&);
	virtual ~ModePageDevice() {}

	virtual bool Dispatch() override;

	virtual void ModeSelect(const DWORD *, const BYTE *, int);

protected:

	int AddModePages(const DWORD *, BYTE *, int);
	virtual void AddModePages(map<int, vector<BYTE>>&, int, bool) const = 0;

private:

	typedef PrimaryDevice super;

	Dispatcher<ModePageDevice> dispatcher;

	virtual int ModeSense6(const DWORD *, BYTE *, int) = 0;
	virtual int ModeSense10(const DWORD *, BYTE *, int) = 0;

	void ModeSense6();
	void ModeSense10();
	void ModeSelect6();
	void ModeSelect10();

	int ModeSelectCheck(int);
	int ModeSelectCheck6();
	int ModeSelectCheck10();
};
