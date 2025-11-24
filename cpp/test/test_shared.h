//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generated/piscsi_interface.pb.h"
#include "shared/scsi.h"
#include <filesystem>
#include <memory>
#include <span>
#include <string>

using namespace std;
using namespace filesystem;
using namespace piscsi_interface;

class PrimaryDevice;
class MockAbstractController;

extern const path test_data_temp_path;

pair<shared_ptr<MockAbstractController>, shared_ptr<PrimaryDevice>> CreateDevice(PbDeviceType, const string& = "");

pair<int, path> OpenTempFile();
path CreateTempFile(int);
path CreateTempFileWithData(span<const byte>);

// create a file with the specified data
void CreateTempFileWithData(const string&, vector<uint8_t>&);

void DeleteTempFile(const string&);

string ReadTempFileToString(const string& filename);

int GetInt16(const vector<byte>&, int);
uint32_t GetInt32(const vector<byte>&, int);

// This class is needed in order to be declared as friend, required to have access to AbstractController::SetCmdByte
class TestInquiry
{
public:
	static void Inquiry(PbDeviceType, scsi_defs::device_type, scsi_defs::scsi_level, const string&, int, bool,
	                    const string& = "");
};
