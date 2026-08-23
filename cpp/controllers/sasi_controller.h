//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2026 Daniel Markstedt <daniel@mindani.net>
//
//---------------------------------------------------------------------------

#pragma once

#include <algorithm>

#include "scsi_controller.h"

class SasiController final : public ScsiController
{
public:
	SasiController(BUS&, int);
	~SasiController() override = default;

	bool IsSasi() const override { return true; }
	int GetEffectiveLun() const override;
	void MsgOut() override;

protected:
	// Use a 100 us baseline for SASI commands rather than the SCSI default
    // of 50 us, needed for timing-sensitive initiators such as the X68000.
	unsigned int GetMinimumExecutionTime() const override { return std::max(100U, MIN_EXEC_TIME); }
};
