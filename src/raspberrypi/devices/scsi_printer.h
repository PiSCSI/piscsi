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
#include <fstream>
#include <string>
#include <unordered_map>

using namespace std;

class SCSIPrinter : public PrimaryDevice, ScsiPrinterCommands //NOSONAR Custom destructor cannot be removed
{
	static const int NOT_RESERVED = -2;

	static constexpr const char *PRINTER_FILE_PATTERN = "/rascsi_sclp-XXXXXX";

public:

	explicit SCSIPrinter(int);
	~SCSIPrinter() override;

	bool Dispatch(scsi_command) override;

	bool Init(const unordered_map<string, string>&) override;
	void Cleanup();

	vector<byte> InquiryInternal() const override;
	void TestUnitReady() override;
	void ReserveUnit() override { PrimaryDevice::ReserveUnit(); }
	void ReleaseUnit() override { PrimaryDevice::ReleaseUnit(); }
	void SendDiagnostic() override { PrimaryDevice::SendDiagnostic(); }
	void Print() override;
	void SynchronizeBuffer();

	bool WriteByteSequence(vector<BYTE>&, uint32_t) override;

private:

	using super = PrimaryDevice;

	Dispatcher<SCSIPrinter> dispatcher;

	string file_template;

	string filename;

	ofstream out;
};
