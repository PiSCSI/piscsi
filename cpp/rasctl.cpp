//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rasctl/rasctl_core.h"

using namespace std;

int main(int argc, char *argv[])
{
	const vector<char *> args(argv, argv + argc);

	return RasCtl().run(args);
}
