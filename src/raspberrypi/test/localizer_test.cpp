//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "rascsi/localizer.h"

TEST(Localizer, Localize)
{
	Localizer localizer;

	string message = localizer.Localize(LocalizationKey::ERROR_AUTHENTICATION, "");
	EXPECT_FALSE(message.empty());
	EXPECT_EQ(string::npos, message.find("enum value"));

	message = localizer.Localize(LocalizationKey::ERROR_AUTHENTICATION, "de_DE");
	EXPECT_FALSE(message.empty());
	EXPECT_EQ(string::npos, message.find("enum value"));

	message = localizer.Localize(LocalizationKey::ERROR_AUTHENTICATION, "en");
	EXPECT_FALSE(message.empty());
	EXPECT_EQ(string::npos, message.find("enum value"));

	message = localizer.Localize((LocalizationKey)1234, "");
	EXPECT_FALSE(message.empty());
	EXPECT_NE(string::npos, message.find("enum value"));
}
