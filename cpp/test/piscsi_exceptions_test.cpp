//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "shared/piscsi_exceptions.h"

using namespace scsi_defs;

TEST(PiscsiExceptionsTest, IoException)
{
	try {
		throw io_exception("msg");
	}
	catch (const io_exception& e) {
		EXPECT_STREQ("msg", e.what());
	}
}

TEST(PiscsiExceptionsTest, FileNotFoundException)
{
	try {
		throw file_not_found_exception("msg");
	}
	catch (const file_not_found_exception& e) {
		EXPECT_STREQ("msg", e.what());
	}
}

TEST(PiscsiExceptionsTest, ScsiErrorException)
{
	try {
		throw scsi_exception(sense_key::unit_attention);
	}
	catch (const scsi_exception& e) {
		EXPECT_EQ(sense_key::unit_attention, e.get_sense_key());
		EXPECT_EQ(asc::no_additional_sense_information, e.get_asc());
	}

	try {
		throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
	}
	catch (const scsi_exception& e) {
		EXPECT_EQ(sense_key::illegal_request, e.get_sense_key());
		EXPECT_EQ(asc::lba_out_of_range, e.get_asc());
	}
}
