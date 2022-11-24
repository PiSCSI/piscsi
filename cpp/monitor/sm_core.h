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
#include "hal/data_sample.h"
#include <memory>
#include <vector>

using namespace std;

// TODO Make static fields/methods non-static
class ScsiMon
{
  public:
    ScsiMon()  = default;
    ~ScsiMon() = default;

    int run(const vector<char *> &);

    inline static double ns_per_loop;

  private:
    void ParseArguments(const vector<char *> &);
    void PrintHelpText(const vector<char *> &) const;
    void Banner() const;
    bool Init();
    void Cleanup() const;
    void Reset() const;

    static void KillHandler(int);

    static inline volatile bool running;

    shared_ptr<BUS> bus;

    uint32_t buff_size = 1000000;

    vector<shared_ptr<DataSample>> data_buffer;

    uint32_t data_idx = 0;

    bool print_help = false;

    bool import_data = false;

    string file_base_name = "log";
    string vcd_file_name;
    string json_file_name;
    string html_file_name;
    string input_file_name;
};
