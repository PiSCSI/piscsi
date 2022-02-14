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
#include <string>
#include <map>

using namespace std;

#define TMP_FILE_PATTERN "/tmp/rascsi_sclp-XXXXXX"

class SCSIPrinter: public PrimaryDevice, ScsiPrinterCommands
{

public:

	SCSIPrinter();
	~SCSIPrinter() {}

	virtual bool Dispatch(SCSIDEV *) override;

	bool Init(const map<string, string>&);

	int Inquiry(const DWORD *, BYTE *) override;
	void TestUnitReady(SASIDEV *) override;
	void ReserveUnit(SASIDEV *);
	void ReleaseUnit(SASIDEV *);
	void Print(SASIDEV *);
	void SynchronizeBuffer(SASIDEV *);
	void SendDiagnostic(SASIDEV *);

	bool Write(BYTE *, uint32_t);
	bool CheckReservation(SASIDEV *);

private:

	typedef PrimaryDevice super;

	Dispatcher<SCSIPrinter> dispatcher;

	char filename[sizeof(TMP_FILE_PATTERN) + 1];
	int fd;

	// The reserving initiator
	// -1 means that the initiator ID is unknown (see SASIDEV), e.g. with Atari ACSI and old host adapters
	int reserving_initiator;
};
