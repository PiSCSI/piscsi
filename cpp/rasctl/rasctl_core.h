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
	// Separator for the INQUIRY name components and for compound parameters
	static const char COMPONENT_SEPARATOR = ':';

public:

	RasCtl() = default;
	~RasCtl() = default;

	int run(const vector<char *>&) const;

private:

	void Banner(const vector<char *>&) const;
};
