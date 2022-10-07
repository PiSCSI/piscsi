//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi.h"
#include "rascsi_interface.pb.h"
#include <string>
#include <memory>

using namespace std;

class PrimaryDevice;
class MockAbstractController;

shared_ptr<PrimaryDevice> CreateDevice(rascsi_interface::PbDeviceType, MockAbstractController&, int);

void TestInquiry(rascsi_interface::PbDeviceType, scsi_defs::device_type, scsi_defs::scsi_level, scsi_defs::scsi_level,
		const string&, int, bool);
