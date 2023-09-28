//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/piscsi_exceptions.h"
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
	const int LUN = 0;
	auto bus = make_shared<MockBus>();
	ControllerManager controller_manager;
	MockAbstractController controller(bus, ID);
	auto device = make_shared<MockSCSIHD_NEC>(LUN);
	device->SetFilename("filename");
	StorageDevice::UnreserveAll();

	EXPECT_TRUE(controller_manager.AttachToController(*bus, ID, device));

	const auto [id1, lun1] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(-1, id1);
	EXPECT_EQ(-1, lun1);

	device->ReserveFile("filename");
	const auto [id2, lun2] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(ID, id2);
	EXPECT_EQ(LUN, lun2);

	device->UnreserveFile();
	const auto [id3, lun3] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(-1, id3);
	EXPECT_EQ(-1, lun3);
}

TEST(StorageDeviceTest, UnreserveAll)
{
	const int ID = 1;
	const int LUN = 0;
	auto bus = make_shared<MockBus>();
	ControllerManager controller_manager;
	MockAbstractController controller(bus, ID);
	auto device = make_shared<MockSCSIHD_NEC>(LUN);
	device->SetFilename("filename");

	EXPECT_TRUE(controller_manager.AttachToController(*bus, ID, device));

	device->ReserveFile("filename");
	StorageDevice::UnreserveAll();
	const auto [id, lun] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);
}

TEST(StorageDeviceTest, GetSetReservedFiles)
{
	const int ID = 1;
	const int LUN = 0;
	auto bus = make_shared<MockBus>();
	ControllerManager controller_manager;
	MockAbstractController controller(bus, ID);
	auto device = make_shared<MockSCSIHD_NEC>(LUN);
	device->SetFilename("filename");

	EXPECT_TRUE(controller_manager.AttachToController(*bus, ID, device));

	device->ReserveFile("filename");

	const unordered_map<string, id_set> reserved_files = StorageDevice::GetReservedFiles();
	EXPECT_EQ(1, reserved_files.size());
	EXPECT_TRUE(reserved_files.contains("filename"));

	StorageDevice::SetReservedFiles(reserved_files);
	EXPECT_EQ(1, reserved_files.size());
	EXPECT_TRUE(reserved_files.contains("filename"));
}

TEST(StorageDeviceTest, FileExists)
{
	EXPECT_FALSE(StorageDevice::FileExists("/non_existing_file"));
	EXPECT_TRUE(StorageDevice::FileExists("/dev/null"));
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
