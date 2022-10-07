//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsi.h"
#include "devices/primary_device.h"
#include "rascsi_interface.pb.h"
#include <string>
#include <memory>

using namespace std;

shared_ptr<PrimaryDevice> CreateDevice(rascsi_interface::PbDeviceType, AbstractController&, int lun);

void TestInquiry(rascsi_interface::PbDeviceType, scsi_defs::device_type, scsi_defs::scsi_level, scsi_defs::scsi_level,
		const string&, int, bool);
