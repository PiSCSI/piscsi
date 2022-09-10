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

	explicit ModePageDevice(const string&);
	~ModePageDevice()override = default;
	ModePageDevice(ModePageDevice&) = delete;
	ModePageDevice& operator=(const ModePageDevice&) = delete;

	bool Dispatch() override;

	vector<BYTE> InquiryInternal() const override = 0;

	virtual void ModeSelect(const DWORD *, const BYTE *, int);

protected:

	int AddModePages(const DWORD *, BYTE *, int) const;
	virtual void AddModePages(map<int, vector<BYTE>>&, int, bool) const = 0;

private:

	using super = PrimaryDevice;

	Dispatcher<ModePageDevice> dispatcher;

	virtual int ModeSense6(const DWORD *, BYTE *, int) = 0;
	virtual int ModeSense10(const DWORD *, BYTE *, int) = 0;

	void ModeSense6();
	void ModeSense10();
	void ModeSelect6();
	void ModeSelect10();

	int ModeSelectCheck(int) const;
	int ModeSelectCheck6() const;
	int ModeSelectCheck10() const;
};
