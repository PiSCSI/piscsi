//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsictl/scsictl_core.h"

using namespace std;

int main(int argc, char *argv[])
{
	const vector<char *> args(argv, argv + argc);

	return ScsiCtl().run(args);
}
