//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 akuker
//
// Simulate a virtual SCSI bus via shared memory on the local host.
//
//---------------------------------------------------------------------------

#include "scsisim/scsisim_core.h"

using namespace std;

int main(int argc, char *argv[])
{
	const vector<char *> args(argv, argv + argc);

	return ScsiSim().run(args);
}
