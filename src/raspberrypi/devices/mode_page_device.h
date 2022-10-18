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

	ModePageDevice(PbDeviceType, int);
	~ModePageDevice() override = default;

	bool Dispatch(scsi_command) override;

	virtual void ModeSelect(const vector<int>&, const vector<BYTE>&, int) const;

protected:

	int AddModePages(const vector<int>&, vector<BYTE>&, int, int, int) const;
	virtual void SetUpModePages(map<int, vector<byte>>&, int, bool) const = 0;

private:

	using super = PrimaryDevice;

	Dispatcher<ModePageDevice> dispatcher;

	virtual int ModeSense6(const vector<int>&, vector<BYTE>&) const = 0;
	virtual int ModeSense10(const vector<int>&, vector<BYTE>&) const = 0;

	void ModeSense6();
	void ModeSense10();
	void ModeSelect6();
	void ModeSelect10();

	int SaveParametersCheck(int) const;
	virtual bool SupportsSaveParameters() const { return false; }
};
