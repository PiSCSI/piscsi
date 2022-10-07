//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsi.h"
#include "rascsi_interface.pb.h"
#include <string>

void TestInquiry(rascsi_interface::PbDeviceType, scsi_defs::device_type, scsi_defs::scsi_level, scsi_defs::scsi_level,
		const std::string&, int, bool);
