//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
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
	static const int NOT_RESERVED = -2;

public:

	SCSIPrinter();
	~SCSIPrinter();

	virtual bool Dispatch() override;

	bool Init(const unordered_map<string, string>&) override;

	vector<BYTE> InquiryInternal() const override;
	void TestUnitReady() override;
	void ReserveUnit() override;
	void ReleaseUnit() override;
	void Print() override;
	void SynchronizeBuffer();
	void SendDiagnostic() override;
	void StopPrint();

	bool WriteByteSequence(BYTE *, uint32_t) override;
	void CheckReservation();
	void DiscardReservation();
	void Cleanup();

private:

	typedef PrimaryDevice super;

	Dispatcher<SCSIPrinter> dispatcher;

	char filename[sizeof(TMP_FILE_PATTERN) + 1];
	int fd = -1;

	int reserving_initiator = NOT_RESERVED;

	time_t reservation_time = 0;
	int timeout = 0;
};
