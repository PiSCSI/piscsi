//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
// Copyright (C) 2022 akuker
//
// Helper method to list devices in a table for rascsi & rasctl
//
//---------------------------------------------------------------------------

#pragma once

#include <list>
#include <string>
#include "rascsi_interface.pb.h"

using namespace std;

namespace ras_util
{
	string ListDevices(const list<rascsi_interface::PbDevice>&);
}
