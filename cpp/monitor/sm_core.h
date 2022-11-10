//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/bus.h"
#include "monitor/data_sample.h"
#include <vector>
#include <memory>

using namespace std;

// TODO Make static fields/methods non-static
class ScsiMon
{
public:

	ScsiMon() = default;
	~ScsiMon() = default;

	int run(const vector<char *>&);

private:

	void ParseArguments(const vector<char *>&);
	void PrintHelpText(const vector<char *>&) const;
	void Banner() const;
	bool Init();
	void Cleanup() const;
	void Reset() const;

	static void KillHandler(int);

	static inline volatile bool running;

	unique_ptr<BUS> bus;

	uint32_t buff_size = 1000000;

	data_capture *data_buffer = nullptr;

	uint32_t data_idx = 0;

	bool print_help = false;

	bool import_data = false;

	string file_base_name = "log";
	string vcd_file_name;
	string json_file_name;
	string html_file_name;
	string input_file_name;

};
