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

	virtual void ModeSelect(scsi_defs::scsi_command, const vector<int>&, const vector<uint8_t>&, int) const;

protected:

	bool SupportsSaveParameters() const { return supports_save_parameters; }
	void SupportsSaveParameters(bool b) { supports_save_parameters = b; }
	int AddModePages(const vector<int>&, vector<uint8_t>&, int, int, int) const;
	virtual void SetUpModePages(map<int, vector<byte>>&, int, bool) const = 0;
	virtual void AddVendorPage(map<int, vector<byte>>&, int, bool) const {
		// Nothing to add by default
	}

private:

	using super = PrimaryDevice;

	Dispatcher<ModePageDevice> dispatcher;

	bool supports_save_parameters = false;

	virtual int ModeSense6(const vector<int>&, vector<uint8_t>&) const = 0;
	virtual int ModeSense10(const vector<int>&, vector<uint8_t>&) const = 0;

	void ModeSense6();
	void ModeSense10();
	void ModeSelect6();
	void ModeSelect10();

	int SaveParametersCheck(int) const;
};
