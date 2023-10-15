//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
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
#include <span>

using namespace std;

class SCSIPrinter : public PrimaryDevice, private ScsiPrinterCommands
{
	static const int NOT_RESERVED = -2;

	static constexpr const char *PRINTER_FILE_PATTERN = "/piscsi_sclp-XXXXXX";

public:

	explicit SCSIPrinter(int);
	~SCSIPrinter() override = default;

	bool Init(const param_map&) override;
	void CleanUp() override;

	vector<uint8_t> InquiryInternal() const override;

	bool WriteByteSequence(span<const uint8_t>) override;

private:

	void TestUnitReady() override;
	void ReserveUnit() override { PrimaryDevice::ReserveUnit(); }
	void ReleaseUnit() override { PrimaryDevice::ReleaseUnit(); }
	void SendDiagnostic() override { PrimaryDevice::SendDiagnostic(); }
	void Print() override;
	void SynchronizeBuffer();

	string file_template;

	string filename;

	ofstream out;
};
