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

class ScsiMon
{
public:

	ScsiMon() = default;
	~ScsiMon() = default;

	int run(const vector<char *>&);

private:

	void parse_arguments(const vector<char *>&);
	void print_copyright_text();
	void print_help_text(const vector<char *>&);
	void Banner();
	bool Init();
	void Cleanup();
	void Reset();
	void FixCpu(int);
};
