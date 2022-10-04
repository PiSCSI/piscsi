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

class SCSIPrinter final : public PrimaryDevice, public ScsiPrinterCommands //NOSONAR Custom destructor cannot be removed
{
	static constexpr const char *TMP_FILE_PATTERN = "/tmp/rascsi_sclp-XXXXXX"; //NOSONAR Using /tmp is safe
	static const int TMP_FILENAME_LENGTH = string_view(TMP_FILE_PATTERN).size();

	static const int NOT_RESERVED = -2;

public:

	explicit SCSIPrinter(int);
	~SCSIPrinter() override;

	bool Dispatch(scsi_command) override;

	bool Init(const unordered_map<string, string>&) override;

	vector<byte> InquiryInternal() const override;
	void TestUnitReady() override;
	void ReserveUnit() override;
	void ReleaseUnit() override;
	void Print() override;
	void SynchronizeBuffer();
	void SendDiagnostic() override;
	void StopPrint();

	bool WriteByteSequence(vector<BYTE>&, uint32_t) override;
	void CheckReservation();
	void DiscardReservation();
	void Cleanup();

private:

	using super = PrimaryDevice;

	Dispatcher<SCSIPrinter> dispatcher;

	char filename[TMP_FILENAME_LENGTH + 1]; //NOSONAR mkstemp() requires a modifiable string
	int fd = -1;

	int reserving_initiator = NOT_RESERVED;

	time_t reservation_time = 0;
	int timeout = 0;
};
