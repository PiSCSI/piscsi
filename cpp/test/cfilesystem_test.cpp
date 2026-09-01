//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
//---------------------------------------------------------------------------

#include "devices/cfilesystem.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

TEST(CFilesystemTest, RetainsHuman68kFilenameConversion)
{
	CHostFilename filename;
	ASSERT_TRUE(filename.SetHost("README.TXT"));

	filename.ConvertHuman();

	EXPECT_TRUE(filename.isCorrect());
	EXPECT_FALSE(filename.isReduce());
	EXPECT_STREQ("README.TXT", (const char*)filename.GetHuman());
}

TEST(CFilesystemTest, RejectsOversizedHostNamesWithoutReplacingTheCurrentName)
{
	CHostFilename filename;
	ASSERT_TRUE(filename.SetHost("valid"));

	const std::string oversized(FILEPATH_MAX, 'a');
	EXPECT_FALSE(filename.SetHost(oversized.c_str()));
	EXPECT_STREQ("valid", filename.GetHost());
}

TEST(CFilesystemTest, RejectsOversizedHostPathAppends)
{
	CHostFiles files;
	ASSERT_TRUE(files.SetResult("/host/"));
	ASSERT_TRUE(files.AddResult("file"));
	EXPECT_STREQ("/host/file", files.GetPath());

	const std::string oversized(FILEPATH_MAX, 'a');
	EXPECT_FALSE(files.AddResult(oversized.c_str()));
	EXPECT_STREQ("/host/file", files.GetPath());
}

TEST(CFilesystemTest, RoundTripsJapaneseHostNamesBetweenUtf8AndShiftJis)
{
	// "日本語.txt" in UTF-8 and Shift-JIS. Use byte strings so the expected
	// Human68k name is independent of the compiler source-file encoding.
	const std::string utf8_name("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E.txt");
	const std::string shift_jis_name("\x93\xFA\x96\x7B\x8C\xEA.txt");
	const auto directory = std::filesystem::temp_directory_path() / "piscsi-cfilesystem-sjis-test"; //NOSONAR test code

	std::error_code error;
	std::filesystem::remove_all(directory, error);
	ASSERT_TRUE(std::filesystem::create_directory(directory, error)) << error.message();
	std::ofstream file(directory / utf8_name);
	ASSERT_TRUE(file.is_open());
	file.close();

	CHostPath path;
	ASSERT_TRUE(path.SetHuman((const uint8_t*)"/"));
	ASSERT_TRUE(path.SetHost((directory.string() + "/").c_str()));
	path.Refresh();

	// Refresh() receives UTF-8 from scandir(), converts it to Shift-JIS for
	// the Human68k cache, then converts the host path back to UTF-8 for stat().
	EXPECT_NE(nullptr, path.FindFilename((const uint8_t*)shift_jis_name.c_str()));

	std::filesystem::remove_all(directory, error);
}
