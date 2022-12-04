//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generated/piscsi_interface.pb.h"
#include <vector>
#include <string>

using namespace std;
using namespace piscsi_interface;

class RasCtl
{
	public:

	RasCtl() = default;
	~RasCtl() = default;

	int run(const vector<char *>&) const;

private:

	void Banner(const vector<char *>&) const;
};
