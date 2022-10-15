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
#include "devices/storage_device.h"
#include <unistd.h>

TEST(StorageDeviceTest, Filename)
{
	MockStorageDevice device;

	device.SetFilename("filename");
	EXPECT_EQ("filename", device.GetFilename());
}

TEST(StorageDeviceTest, ValidateFile)
{
	MockStorageDevice device;

	device.SetBlockCount(0);
	EXPECT_THROW(device.ValidateFile("/non_existing_file"), io_exception);

	device.SetReadOnly(false);
	device.SetProtectable(true);
	device.SetBlockCount(1);
	device.ValidateFile("/non_existing_file");
	EXPECT_TRUE(device.IsReadOnly());
	EXPECT_FALSE(device.IsProtectable());
	EXPECT_FALSE(device.IsStopped());
	EXPECT_FALSE(device.IsRemoved());
	EXPECT_FALSE(device.IsLocked());

	device.SetReadOnly(false);
	device.SetProtectable(true);
	device.ValidateFile("/dev/null");
	EXPECT_FALSE(device.IsReadOnly());
	EXPECT_TRUE(device.IsProtectable());
	EXPECT_FALSE(device.IsStopped());
	EXPECT_FALSE(device.IsRemoved());
	EXPECT_FALSE(device.IsLocked());
}

TEST(StorageDeviceTest, MediumChanged)
{
	MockStorageDevice device;

	device.SetRemovable(true);
	EXPECT_FALSE(device.IsMediumChanged());
	device.MediumChanged();
	EXPECT_TRUE(device.IsMediumChanged()) << "Medium change not reported";

	device.SetMediumChanged(true);
	EXPECT_TRUE(device.IsMediumChanged());
	device.SetMediumChanged(false);
	EXPECT_FALSE(device.IsMediumChanged());
}

TEST(StorageDeviceTest, GetIdsForReservedFile)
{
	const int ID = 1;
	const int LUN = 2;

	MockStorageDevice device;
	device.SetFilename("filename");

	int id;
	int lun;
	EXPECT_FALSE(StorageDevice::GetIdsForReservedFile("filename", id, lun));

	device.ReserveFile("filename", ID, LUN);
	EXPECT_TRUE(StorageDevice::GetIdsForReservedFile("filename", id, lun));
	EXPECT_EQ(ID, id);
	EXPECT_EQ(LUN, lun);

	device.UnreserveFile();
	EXPECT_FALSE(StorageDevice::GetIdsForReservedFile("filename", id, lun));
}

TEST(StorageDeviceTest, UnreserveAll)
{
	MockStorageDevice device;
	device.ReserveFile("filename", 2, 31);

	StorageDevice::UnreserveAll();
	int id;
	int lun;
	EXPECT_FALSE(StorageDevice::GetIdsForReservedFile("filename", id, lun));
}

TEST(StorageDeviceTest, GetSetReservedFiles)
{
	const int ID = 1;
	const int LUN = 16;

	MockStorageDevice device;
	device.ReserveFile("filename", ID, LUN);

	const unordered_map<string, id_set> reserved_files = StorageDevice::GetReservedFiles();
	EXPECT_EQ(1, reserved_files.size());
	EXPECT_NE(reserved_files.end(), reserved_files.find("filename"));

	StorageDevice::SetReservedFiles(reserved_files);
	EXPECT_EQ(1, reserved_files.size());
	EXPECT_NE(reserved_files.end(), reserved_files.find("filename"));
}

TEST(StorageDeviceTest, FileExists)
{
	EXPECT_FALSE(StorageDevice::FileExists("/non_existing_file"));
	EXPECT_TRUE(StorageDevice::FileExists("/dev/null"));
}

TEST(StorageDeviceTest, IsReadOnlyFile)
{
	MockStorageDevice device;

	device.SetFilename("/dev/null");
	EXPECT_FALSE(device.IsReadOnlyFile());

	device.SetFilename("/dev/mem");
	EXPECT_TRUE(device.IsReadOnlyFile());
}

TEST(StorageDeviceTest, GetFileSize)
{
	MockStorageDevice device;

	const string filename = CreateTempFile(512);
	device.SetFilename(filename);
	const off_t size = device.GetFileSize();
	unlink(filename.c_str());
	EXPECT_EQ(512, size);

	device.SetFilename("/non_existing_file");
	EXPECT_THROW(device.GetFileSize(), io_exception);
}
