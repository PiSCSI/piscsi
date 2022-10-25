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
#include <filesystem>

using namespace filesystem;

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
	device.SetFilename("/non_existing_file");
	EXPECT_THROW(device.ValidateFile(), io_exception);

	device.SetBlockCount(1);
	EXPECT_THROW(device.ValidateFile(), io_exception);

	const path filename = CreateTempFile(1);
	device.SetFilename(string(filename));
	device.SetReadOnly(false);
	device.SetProtectable(true);
	device.ValidateFile();
	EXPECT_FALSE(device.IsReadOnly());
	EXPECT_TRUE(device.IsProtectable());
	EXPECT_FALSE(device.IsStopped());
	EXPECT_FALSE(device.IsRemoved());
	EXPECT_FALSE(device.IsLocked());

	permissions(filename, perms::owner_read);
	device.SetReadOnly(false);
	device.SetProtectable(true);
	device.ValidateFile();
	EXPECT_TRUE(device.IsReadOnly());
	EXPECT_FALSE(device.IsProtectable());
	EXPECT_FALSE(device.IsProtected());
	EXPECT_FALSE(device.IsStopped());
	EXPECT_FALSE(device.IsRemoved());
	EXPECT_FALSE(device.IsLocked());

	remove(filename);
}

TEST(StorageDeviceTest, MediumChanged)
{
	MockStorageDevice device;

	device.SetMediumChanged(true);
	EXPECT_TRUE(device.IsMediumChanged());

	device.SetMediumChanged(false);
	EXPECT_FALSE(device.IsMediumChanged());
}

TEST(StorageDeviceTest, GetIdsForReservedFile)
{
	const int ID = 1;
	const int LUN = 2;
	StorageDevice::UnreserveAll();

	MockStorageDevice device;
	device.SetFilename("filename");

	const auto [id1, lun1] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(-1, id1);
	EXPECT_EQ(-1, lun1);

	device.ReserveFile("filename", ID, LUN);
	const auto [id2, lun2] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(ID, id2);
	EXPECT_EQ(LUN, lun2);

	device.UnreserveFile();
	const auto [id3, lun3] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(-1, id3);
	EXPECT_EQ(-1, lun3);
}

TEST(StorageDeviceTest, UnreserveAll)
{
	MockStorageDevice device;
	device.ReserveFile("filename", 2, 31);

	StorageDevice::UnreserveAll();
	const auto [id, lun] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);
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

	const path filename = CreateTempFile(512);
	device.SetFilename(filename.c_str());
	const off_t size = device.GetFileSize();
	remove(filename);
	EXPECT_EQ(512, size);

	device.SetFilename("/non_existing_file");
	EXPECT_THROW(device.GetFileSize(), io_exception);
}

TEST(StorageDeviceTest, Dispatch)
{
    MockAbstractController controller(make_shared<MockBus>(), 0);
	auto device = make_shared<NiceMock<MockStorageDevice>>();

    controller.AddDevice(device);

    EXPECT_FALSE(device->Dispatch(scsi_command::eCmdIcd)) << "Command is not supported by this class";
}

