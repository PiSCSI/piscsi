//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "scsictl/scsictl_parser.h"

TEST(ScsictlParserTest, ParseOperation)
{
	ScsictlParser parser;

	EXPECT_EQ(ATTACH, parser.ParseOperation("A"));
	EXPECT_EQ(ATTACH, parser.ParseOperation("a"));
	EXPECT_EQ(DETACH, parser.ParseOperation("d"));
	EXPECT_EQ(INSERT, parser.ParseOperation("i"));
	EXPECT_EQ(EJECT, parser.ParseOperation("e"));
	EXPECT_EQ(PROTECT, parser.ParseOperation("p"));
	EXPECT_EQ(UNPROTECT, parser.ParseOperation("u"));
	EXPECT_EQ(NO_OPERATION, parser.ParseOperation(""));
	EXPECT_EQ(NO_OPERATION, parser.ParseOperation("xyz"));
}

TEST(ScsictlParserTest, ParseType)
{
	ScsictlParser parser;

	EXPECT_EQ(SCCD, parser.ParseType("SCCD"));
	EXPECT_EQ(SCCD, parser.ParseType("sccd"));
	EXPECT_EQ(SCDP, parser.ParseType("scdp"));
	EXPECT_EQ(SCHD, parser.ParseType("schd"));
	EXPECT_EQ(SCHS, parser.ParseType("schs"));
	EXPECT_EQ(SCLP, parser.ParseType("sclp"));
	EXPECT_EQ(SCMO, parser.ParseType("scmo"));
	EXPECT_EQ(SCRM, parser.ParseType("scrm"));
	EXPECT_EQ(SCTP, parser.ParseType("sctp"));

	EXPECT_EQ(SCCD, parser.ParseType("C"));
	EXPECT_EQ(SCCD, parser.ParseType("c"));
	EXPECT_EQ(SCDP, parser.ParseType("d"));
	EXPECT_EQ(SCHD, parser.ParseType("h"));
	EXPECT_EQ(SCHS, parser.ParseType("s"));
	EXPECT_EQ(SCLP, parser.ParseType("p"));
	EXPECT_EQ(SCMO, parser.ParseType("m"));
	EXPECT_EQ(SCRM, parser.ParseType("r"));
	EXPECT_EQ(SCTP, parser.ParseType("t"));

	EXPECT_EQ(UNDEFINED, parser.ParseType(""));
	EXPECT_EQ(UNDEFINED, parser.ParseType("xyz"));
}
