//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 akuker
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

#include "shared/shared_memory.h"
#include "hal/data_sample.h"

using namespace std;

class ScsiSim
{
  public:
    ScsiSim()  = default;
    ~ScsiSim() = default;

    int run(const vector<char*>& args);

  private:
    void Banner(const vector<char*>&) const;
    static void TerminationHandler(int signum);
    bool ParseArgument(const vector<char*>&);

    int InitSharedMemory();
    void TeardownSharedMemory();

    void PrintDifferences(const DataSample &current, const DataSample &previous);
    void TestClient();

    bool enable_debug = false;

    unique_ptr<SharedMemory> signals;

    uint32_t prev_data;

    bool running;

    bool test_mode = false;
};
