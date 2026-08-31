//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "hal/connection_profile.h"
#include "scsimon/sm_core.h"

#include <iostream>

using namespace std;

int main(int argc, char* argv[])
{
    const vector<char*> args(argv, argv + argc);
    vector<char*> configured_args = args;
    string error;
    if (!ConfigureConnectionType(configured_args, error)) {
        cerr << error << '\n';
        return EXIT_FAILURE;
    }

    return ScsiMon().run(configured_args);
}
