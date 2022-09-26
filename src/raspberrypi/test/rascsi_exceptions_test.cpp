//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "rascsi_exceptions.h"

using namespace scsi_defs;

TEST(RascsiExceptionsTest, IoException)
{
	try {
		throw io_exception("msg");
	}
	catch(const io_exception& e) {
		EXPECT_EQ("msg", e.get_msg());
	}
}

TEST(RascsiExceptionsTest, ScsiErrorException)
{
	try {
		throw scsi_error_exception();
	}
	catch(const scsi_error_exception& e) {
		EXPECT_EQ(sense_key::ABORTED_COMMAND, e.get_sense_key());
		EXPECT_EQ(asc::NO_ADDITIONAL_SENSE_INFORMATION, e.get_asc());
		EXPECT_EQ(status::CHECK_CONDITION, e.get_status());
	}

	try {
		throw scsi_error_exception(sense_key::UNIT_ATTENTION);
	}
	catch(const scsi_error_exception& e) {
		EXPECT_EQ(sense_key::UNIT_ATTENTION, e.get_sense_key());
		EXPECT_EQ(asc::NO_ADDITIONAL_SENSE_INFORMATION, e.get_asc());
		EXPECT_EQ(status::CHECK_CONDITION, e.get_status());
	}

	try {
		throw scsi_error_exception(sense_key::UNIT_ATTENTION, asc::LBA_OUT_OF_RANGE);
	}
	catch(const scsi_error_exception& e) {
		EXPECT_EQ(sense_key::UNIT_ATTENTION, e.get_sense_key());
		EXPECT_EQ(asc::LBA_OUT_OF_RANGE, e.get_asc());
		EXPECT_EQ(status::CHECK_CONDITION, e.get_status());
	}

	try {
		throw scsi_error_exception(sense_key::UNIT_ATTENTION, asc::LBA_OUT_OF_RANGE, status::BUSY);
	}
	catch(const scsi_error_exception& e) {
		EXPECT_EQ(sense_key::UNIT_ATTENTION, e.get_sense_key());
		EXPECT_EQ(asc::LBA_OUT_OF_RANGE, e.get_asc());
		EXPECT_EQ(status::BUSY, e.get_status());
	}
}
