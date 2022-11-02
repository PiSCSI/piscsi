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

class SCSIPrinter : public PrimaryDevice, private ScsiPrinterCommands
{
	static const int NOT_RESERVED = -2;

	static constexpr const char *PRINTER_FILE_PATTERN = "/rascsi_sclp-XXXXXX";

public:

	explicit SCSIPrinter(int lun) : PrimaryDevice(SCLP, lun) {}
	~SCSIPrinter() override = default;

	bool Init(const unordered_map<string, string>&) override;

	vector<uint8_t> InquiryInternal() const override;

	bool WriteByteSequence(vector<uint8_t>&, uint32_t) override;

private:

	void TestUnitReady() override;
	void ReserveUnit() override { PrimaryDevice::ReserveUnit(); }
	void ReleaseUnit() override { PrimaryDevice::ReleaseUnit(); }
	void SendDiagnostic() override { PrimaryDevice::SendDiagnostic(); }
	void Print() override;
	void SynchronizeBuffer();

	void Cleanup();

	string file_template;

	string filename;

	ofstream out;
};
