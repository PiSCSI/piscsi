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

#include "sasidump/sasidump_core.h"

using namespace std;

int main(int argc, char* argv[])
{
    vector<char*> args(argv, argv + argc);

    return SasiDump().run(args);
}
