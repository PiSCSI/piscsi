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
#include <unordered_map>

using namespace std;

#define TMP_FILE_PATTERN "/tmp/rascsi_sclp-XXXXXX"

class SCSIPrinter: public PrimaryDevice, ScsiPrinterCommands
{

public:

	SCSIPrinter();
	~SCSIPrinter();

	virtual bool Dispatch(Controller *) override;

	bool Init(const unordered_map<string, string>&);

	vector<BYTE> Inquiry() const override;
	void TestUnitReady(Controller *);
	void ReserveUnit(Controller *);
	void ReleaseUnit(Controller *);
	void Print(Controller *);
	void SynchronizeBuffer(Controller *);
	void SendDiagnostic(Controller *);
	void StopPrint(Controller *);

	bool WriteBytes(BYTE *, uint32_t) override;
	bool CheckReservation(Controller *);
	void DiscardReservation();
	void Cleanup();

private:

	typedef PrimaryDevice super;

	Dispatcher<SCSIPrinter, Controller> dispatcher;

	char filename[sizeof(TMP_FILE_PATTERN) + 1];
	int fd;

	int reserving_initiator;

	time_t reservation_time;
	int timeout;
};
