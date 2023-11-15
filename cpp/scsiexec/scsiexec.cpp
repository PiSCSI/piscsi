//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsisexec_core.h"

using namespace std;

int main(int argc, char *argv[])
{
	vector<char *> args(argv, argv + argc);

	return ScsiExec().run(args);
}
