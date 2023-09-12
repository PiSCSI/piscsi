//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "piscsi/piscsi_core.h"

using namespace std;

TEST(PiscsiCoreTest, Run)
{
	vector<char *> args;

	args.emplace_back((char *)"-v"); //NOSONAR Required to match run() method signature
	EXPECT_EQ(EXIT_SUCCESS, Piscsi().run(args));
}
