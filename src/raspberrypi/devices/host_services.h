//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------
#pragma once

#include "disk.h"

class HostServices: public Disk
{
public:
	HostServices() : Disk("SCHS") {};
	~HostServices() {};

	virtual bool Dispatch(SCSIDEV *) override;

	int Inquiry(const DWORD *, BYTE *) override;
	void TestUnitReady(SASIDEV *) override;
	void StartStopUnit(SASIDEV *) override;
	int AddVendorPage(int, bool, BYTE *) override;
};
