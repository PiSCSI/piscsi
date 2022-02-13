//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------
#pragma once

#include "primary_device.h"

class SCSIPrinter: public PrimaryDevice
{

public:

	SCSIPrinter();
	~SCSIPrinter() {}

	virtual bool Dispatch(SCSIDEV *) override;

	int Inquiry(const DWORD *, BYTE *) override;
	void TestUnitReady(SASIDEV *) override;
	void ReserveUnit(SASIDEV *);
	void ReleaseUnit(SASIDEV *);
	void Print(SASIDEV *);
	void SynchronizeBuffer(SASIDEV *);
	void SendDiagnostic(SASIDEV *);

private:

	typedef PrimaryDevice super;

	Dispatcher<SCSIPrinter> dispatcher;
};
