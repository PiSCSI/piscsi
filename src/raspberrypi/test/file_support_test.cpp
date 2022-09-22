//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "devices/file_support.h"

class TestFileSupport : public FileSupport
{
	void Open(const Filepath&) final {
		// Do nothing when running unit tests
	}
};

TEST(FileSupportTest, Reserve)
{
	const int ID = 1;
	const int LUN = 2;

	Filepath path;
	path.SetPath("path");
	TestFileSupport file_support;

	file_support.SetPath(path);
	Filepath result;
	file_support.GetPath(result);
	EXPECT_STREQ("path", result.GetPath());

	int id;
	int lun;
	EXPECT_FALSE(FileSupport::GetIdsForReservedFile(path, id, lun));

	file_support.ReserveFile(path, ID, LUN);
	EXPECT_TRUE(FileSupport::GetIdsForReservedFile(path, id, lun));
	EXPECT_EQ(ID, id);
	EXPECT_EQ(LUN, lun);

	file_support.UnreserveFile();
	EXPECT_FALSE(FileSupport::GetIdsForReservedFile(path, id, lun));
}

TEST(FileSupportTest, UnreserveAll)
{
	const int ID = 2;
	const int LUN = 31;

	Filepath path;
	path.SetPath("path");
	TestFileSupport file_support;

	file_support.ReserveFile(path, ID, LUN);
	FileSupport::UnreserveAll();
	int id;
	int lun;
	EXPECT_FALSE(FileSupport::GetIdsForReservedFile(path, id, lun));
}
