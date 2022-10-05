//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
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

using namespace std;

namespace ras_util
{
	void Banner(const string&);
	bool GetAsInt(const string&, int&);
	string ListDevices(const list<rascsi_interface::PbDevice>&);

	void FixCpu(int);
}
