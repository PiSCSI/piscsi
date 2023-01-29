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
    bool SetLogLevel(const string&);

    int InitSharedMemory();
    void TeardownSharedMemory();

    bool enable_debug = false;

    unique_ptr<SharedMemory> signals;

    bool running;
};
