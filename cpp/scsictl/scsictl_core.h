//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generated/piscsi_interface.pb.h"
#include <vector>
#include <string>

using namespace std;
using namespace piscsi_interface;

class ScsiCtl
{
	public:

	ScsiCtl() = default;
	~ScsiCtl() = default;

	int run(const vector<char *>&) const;

private:

	void Banner(const vector<char *>&) const;

    int ExportAsBinary(const PbCommand&, const string&) const;
    int ExportAsJson(const PbCommand&, const string&) const;
    int ExportAsText(const PbCommand&, const string&) const;
};
