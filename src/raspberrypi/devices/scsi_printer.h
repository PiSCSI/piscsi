//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Implementation of a SCSI printer (see SCSI-2 specification for a command description)
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
	~SCSIPrinter();

	virtual bool Dispatch(SCSIDEV *) override;

	bool Init(const map<string, string>&);

	int Inquiry(const DWORD *, BYTE *) override;
	void TestUnitReady(SCSIDEV *);
	void ReserveUnit(SCSIDEV *);
	void ReleaseUnit(SCSIDEV *);
	void Print(SCSIDEV *);
	void SynchronizeBuffer(SCSIDEV *);
	void SendDiagnostic(SCSIDEV *);
	void StopPrint(SCSIDEV *);

	bool WriteBytes(BYTE *, uint32_t) override;
	bool CheckReservation(SCSIDEV *);
	void DiscardReservation();
	void Cleanup();

private:

	typedef PrimaryDevice super;

	Dispatcher<SCSIPrinter, SCSIDEV> dispatcher;

	char filename[sizeof(TMP_FILE_PATTERN) + 1];
	int fd;

	int reserving_initiator;

	time_t reservation_time;
	int timeout;
};
