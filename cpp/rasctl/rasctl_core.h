//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <vector>

using namespace std;

class RasCtl
{
	public:

	RasCtl() = default;
	~RasCtl() = default;

	int run(const vector<char *>&) const;

private:

	void Banner(const vector<char *>&) const;
};
