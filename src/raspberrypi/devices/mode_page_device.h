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

class ModePageDevice: public PrimaryDevice
{
public:

	explicit ModePageDevice(const string&);
	~ModePageDevice()override = default;
	ModePageDevice(ModePageDevice&) = delete;
	ModePageDevice& operator=(const ModePageDevice&) = delete;

	bool Dispatch(scsi_command) override;

	virtual void ModeSelect(const vector<int>&, const BYTE *, int) const;

protected:

	int AddModePages(const vector<int>&, BYTE *, int) const;
	virtual void AddModePages(map<int, vector<byte>>&, int, bool) const = 0;

private:

	using super = PrimaryDevice;

	Dispatcher<ModePageDevice> dispatcher;

	virtual int ModeSense6(const vector<int>&, BYTE *, int) const = 0;
	virtual int ModeSense10(const vector<int>&, BYTE *, int) const = 0;

	void ModeSense6();
	void ModeSense10();
	void ModeSelect6();
	void ModeSelect10();

	int ModeSelectCheck(int) const;
	int ModeSelectCheck6() const;
	int ModeSelectCheck10() const;
};
