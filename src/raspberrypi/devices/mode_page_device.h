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

using namespace std;

class ModePageDevice: public PrimaryDevice
{
public:

	ModePageDevice(const string);
	virtual ~ModePageDevice() {}

	virtual bool Dispatch(SCSIDEV *) override;

	virtual int ModeSense6(const DWORD *, BYTE *) = 0;
	virtual int ModeSense10(const DWORD *, BYTE *) = 0;

	// TODO This method should not be called by SASIDEV
	virtual bool ModeSelect(const DWORD *, const BYTE *, int);

private:

	Dispatcher<ModePageDevice> dispatcher;

	void ModeSense6(SASIDEV *) override;
	void ModeSense10(SASIDEV *) override;
	void ModeSelect6(SASIDEV *) override;
	void ModeSelect10(SASIDEV *) override;

	int ModeSelectCheck(const DWORD *, int);
	int ModeSelectCheck6(const DWORD *);
	int ModeSelectCheck10(const DWORD *);
};
