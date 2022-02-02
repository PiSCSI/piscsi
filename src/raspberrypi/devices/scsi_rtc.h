//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "disk.h"
#include "../rascsi.h"

class SCSIRtc: public Disk
{
public:
	SCSIRtc() : Disk("SCRT") {};
	~SCSIRtc() {};

	virtual bool Dispatch(SCSIDEV *) override;

	int Inquiry(const DWORD *, BYTE *) override;
	void TestUnitReady(SASIDEV *) override;
	int AddVendorPage(int, bool, BYTE *) override;
};
