//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/piscsi_util.h"
#include "shared/piscsi_exceptions.h"
#include "devices/storage_device.h"
#include <filesystem>

using namespace filesystem;

class StorageDeviceTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Clean up any previous test state before each test
		StorageDevice::UnreserveAll();
	}

	void TearDown() override {
		// Clean up after each test
		StorageDevice::UnreserveAll();
	}
};

TEST_F(StorageDeviceTest, SetGetFilename)
{
	MockStorageDevice device;

	device.SetFilename("filename");
	EXPECT_EQ("filename", device.GetFilename());
}

TEST_F(StorageDeviceTest, ValidateFile)
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

TEST_F(StorageDeviceTest, MediumChanged)
{
	MockStorageDevice device;

	EXPECT_FALSE(device.IsMediumChanged());

	device.SetMediumChanged(true);
	EXPECT_TRUE(device.IsMediumChanged());

	device.SetMediumChanged(false);
	EXPECT_FALSE(device.IsMediumChanged());
}

TEST_F(StorageDeviceTest, GetIdsForReservedFile)
{
	const int ID = 1;
	const int LUN = 0;
	auto bus = make_shared<MockBus>();
	ControllerManager controller_manager;
	MockAbstractController controller(bus, ID);
	auto device = make_shared<MockSCSIHD_NEC>(LUN);
	device->SetFilename("filename");

	EXPECT_TRUE(controller_manager.AttachToController(*bus, ID, device));

	const auto [id1, lun1] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(-1, id1);
	EXPECT_EQ(-1, lun1);

	device->ReserveFile();
	const auto [id2, lun2] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(ID, id2);
	EXPECT_EQ(LUN, lun2);

	device->UnreserveFile();
	const auto [id3, lun3] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(-1, id3);
	EXPECT_EQ(-1, lun3);
}

TEST_F(StorageDeviceTest, UnreserveAll)
{
	const int ID = 1;
	const int LUN = 0;
	auto bus = make_shared<MockBus>();
	ControllerManager controller_manager;
	MockAbstractController controller(bus, ID);
	auto device = make_shared<MockSCSIHD_NEC>(LUN);
	device->SetFilename("filename");

	EXPECT_TRUE(controller_manager.AttachToController(*bus, ID, device));

	device->ReserveFile();
	StorageDevice::UnreserveAll();
	const auto [id, lun] = StorageDevice::GetIdsForReservedFile("filename");
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);
}

TEST_F(StorageDeviceTest, GetSetReservedFiles)
{
	const int ID = 1;
	const int LUN = 0;
	auto bus = make_shared<MockBus>();
	ControllerManager controller_manager;
	MockAbstractController controller(bus, ID);
	auto device = make_shared<MockSCSIHD_NEC>(LUN);
	device->SetFilename("filename");

	EXPECT_TRUE(controller_manager.AttachToController(*bus, ID, device));

	device->ReserveFile();

	const auto& reserved_files = StorageDevice::GetReservedFiles();
	EXPECT_EQ(1, reserved_files.size());
	EXPECT_TRUE(reserved_files.contains("filename"));

	StorageDevice::SetReservedFiles(reserved_files);
	EXPECT_EQ(1, reserved_files.size());
	EXPECT_TRUE(reserved_files.contains("filename"));
}

TEST_F(StorageDeviceTest, FileExists)
{
	EXPECT_FALSE(StorageDevice::FileExists("/non_existing_file"));
	EXPECT_TRUE(StorageDevice::FileExists("/dev/null"));
}

TEST_F(StorageDeviceTest, GetFileSize)
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
