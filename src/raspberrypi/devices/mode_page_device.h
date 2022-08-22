//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
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

	virtual bool Dispatch(Controller *) override;

	virtual int ModeSense6(const DWORD *, BYTE *) = 0;
	virtual int ModeSense10(const DWORD *, BYTE *, int) = 0;

	// TODO This method should not be called by Controller
	virtual bool ModeSelect(const DWORD *, const BYTE *, int);

protected:

	int AddModePages(const DWORD *, BYTE *, int);
	virtual void AddModePages(map<int, vector<BYTE>>&, int, bool) const = 0;

private:

	typedef PrimaryDevice super;

	Dispatcher<ModePageDevice, Controller> dispatcher;

	void ModeSense6(Controller *);
	void ModeSense10(Controller *);
	void ModeSelect6(Controller *);
	void ModeSelect10(Controller *);

	int ModeSelectCheck(int);
	int ModeSelectCheck6();
	int ModeSelectCheck10();
};
