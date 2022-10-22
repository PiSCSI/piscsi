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
	bool GetAsInt(const string&, int&);
	string Banner(const string&);
	string ListDevices(const list<rascsi_interface::PbDevice>&);

	void FixCpu(int);
}
