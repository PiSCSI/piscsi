//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------
#pragma once

#include "interfaces/scsi_printer_commands.h"
#include "primary_device.h"

class SCSIPrinter: public PrimaryDevice, ScsiPrinterCommands
{

public:

	SCSIPrinter();
	~SCSIPrinter() {}

	virtual bool Dispatch(SCSIDEV *) override;

	int Inquiry(const DWORD *, BYTE *) override;
	void ReserveUnit(SASIDEV *);
	void ReleaseUnit(SASIDEV *);
	void Print(SASIDEV *);
	void SynchronizeBuffer(SASIDEV *);
	void SendDiagnostic(SASIDEV *);

	bool Write(BYTE *, uint32_t);

private:

	typedef PrimaryDevice super;

	Dispatcher<SCSIPrinter> dispatcher;

	FILE *lp_file;
};
