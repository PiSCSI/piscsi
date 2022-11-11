//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generated/rascsi_interface.pb.h"
#include <vector>
#include <string>

using namespace std;
using namespace rascsi_interface;

class RasCtl
{
	public:

	RasCtl() = default;
	~RasCtl() = default;

	int run(const vector<char *>&) const;

private:

	void Banner(const vector<char *>&) const;
};
