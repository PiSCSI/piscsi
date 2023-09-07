//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "primary_device.h"
#include <string>
#include <span>
#include <vector>
#include <map>

class ModePageDevice : public PrimaryDevice
{
public:

	using PrimaryDevice::PrimaryDevice;

	bool Init(const unordered_map<string, string>&) override;

	virtual void ModeSelect(scsi_defs::scsi_command, span<const int>, span<const uint8_t>, int) const;

protected:

	bool SupportsSaveParameters() const { return supports_save_parameters; }
	void SupportsSaveParameters(bool b) { supports_save_parameters = b; }
	int AddModePages(span<const int>, vector<uint8_t>&, int, int, int) const;
	virtual void SetUpModePages(map<int, vector<byte>>&, int, bool) const = 0;
	virtual void AddVendorPage(map<int, vector<byte>>&, int, bool) const {
		// Nothing to add by default
	}

private:

	bool supports_save_parameters = false;

	virtual int ModeSense6(span<const int>, vector<uint8_t>&) const = 0;
	virtual int ModeSense10(span<const int>, vector<uint8_t>&) const = 0;

	void ModeSense6() const;
	void ModeSense10() const;
	void ModeSelect6() const;
	void ModeSelect10() const;

	void SaveParametersCheck(int) const;
};
