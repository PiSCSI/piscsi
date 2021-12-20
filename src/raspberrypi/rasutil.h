//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Helper methods used by rascsi and rasctl
//
//---------------------------------------------------------------------------

#pragma once

#include <list>
#include <string>
#include "rascsi_interface.pb.h"

namespace ras_util
{
	bool GetAsInt(const std::string&, int&);
	std::string ListDevices(const std::list<rascsi_interface::PbDevice>&);
}
