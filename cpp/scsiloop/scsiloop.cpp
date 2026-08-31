//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "hal/connection_profile.h"
#include "scsiloop/scsiloop_core.h"

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

    return ScsiLoop().run(args);
}
