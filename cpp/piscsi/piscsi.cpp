//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "piscsi/piscsi_core.h"

using namespace std;

int main(int argc, char *argv[])
{
	vector<char *> args(argv, argv + argc);

	return Piscsi().run(args);
}
