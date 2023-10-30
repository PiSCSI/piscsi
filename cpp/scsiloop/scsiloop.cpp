//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "scsiloop/scsiloop_core.h"

using namespace std;

int main(int argc, char *argv[])
{
	const vector<char *> args(argv, argv + argc);

	return ScsiLoop().run(args);
}
