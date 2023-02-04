//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#pragma once

#include <map>
#include <string>
#include <vector>

using namespace std;

class ScsiLoop
{
  public:
    ScsiLoop()  = default;
    ~ScsiLoop() = default;

    int run(const vector<char *> &args);

  private:
    void Banner(const vector<char *> &) const;
    static void TerminationHandler(int signum);
    bool ParseArgument(const vector<char *> &);
    bool SetLogLevel(const string &);
};
