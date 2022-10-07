//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "rascsi_exceptions.h"
#include "devices/scsihd.h"

TEST(ScsiHdTest, FinalizeSetup)
{
	MockSCSIHD_NEC disk(0);
	Filepath filepath;

	EXPECT_THROW(disk.FinalizeSetup(filepath, 2LL * 1024 * 1024 * 1024 * 1024 + 1, 0), io_exception);
}

TEST(ScsiHdTest, Inquiry)
{
	TestInquiry(SCHD, device_type::DIRECT_ACCESS, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"RaSCSI                  ", 0x1f, false);
}
