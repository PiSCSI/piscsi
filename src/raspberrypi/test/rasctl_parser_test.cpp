//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "rasctl/rasctl_parser.h"

TEST(RasctlParserTest, ParseOperation)
{
	RasctlParser parser;

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

TEST(RasctlParserTest, ParseType)
{
	RasctlParser parser;

	EXPECT_EQ(SCBR, parser.ParseType("B"));
	EXPECT_EQ(SCBR, parser.ParseType("b"));
	EXPECT_EQ(SCCD, parser.ParseType("c"));
	EXPECT_EQ(SCDP, parser.ParseType("d"));
	EXPECT_EQ(SCHD, parser.ParseType("h"));
	EXPECT_EQ(SCLP, parser.ParseType("l"));
	EXPECT_EQ(SCMO, parser.ParseType("m"));
	EXPECT_EQ(SCRM, parser.ParseType("r"));
	EXPECT_EQ(SCHS, parser.ParseType("s"));
	EXPECT_EQ(UNDEFINED, parser.ParseType(""));
	EXPECT_EQ(UNDEFINED, parser.ParseType("xyz"));
}
