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

class SCSIRascsi: public Disk
{
public:
	SCSIRascsi() : Disk("SCRT") {};
	~SCSIRascsi() {};

	virtual bool Dispatch(SCSIDEV *) override;

	int Inquiry(const DWORD *, BYTE *) override;
	void TestUnitReady(SASIDEV *) override;
	int AddVendorPage(int, bool, BYTE *) override;
};
