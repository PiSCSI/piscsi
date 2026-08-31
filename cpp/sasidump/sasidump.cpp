//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2026 Daniel Markstedt <daniel@mindani.net>
//
//---------------------------------------------------------------------------

#include "hal/connection_profile.h"
#include "sasidump/sasidump_core.h"

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

    return SasiDump().run(args);
}
