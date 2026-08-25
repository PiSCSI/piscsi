//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2026 Daniel Markstedt <daniel@mindani.net>
//
//---------------------------------------------------------------------------

#pragma once

#include "disk.h"

class SasiHd final : public Disk
{
public:
	explicit SasiHd(int, const unordered_set<uint32_t>& = { 256, 512, 1024 });
	~SasiHd() override = default;

	bool Init(const param_map&) override;
	void Open() override;

	void Inquiry() override;
	void RequestSense() override;

private:
	vector<uint8_t> InquiryInternal() const override { return {}; }
	void TestUnitReady() override;
	void AssignDiskParameters();
	void ReadCapacity();
};
