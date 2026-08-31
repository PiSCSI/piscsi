//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "hal/connection_profile.h"
#include "scsidump/scsidump_core.h"

#include <iostream>

using namespace std;

int main(int argc, char* argv[])
{
    vector<char*> args(argv, argv + argc);
    string error;
    if (!ConfigureConnectionType(args, error)) {
        cerr << error << '\n';
        return EXIT_FAILURE;
    }

    return ScsiDump().run(args);
}
