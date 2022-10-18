//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "scsi.h"
#include "devices/disk.h"
#include "devices/scsi_command_util.h"
#include "rascsi_exceptions.h"

using namespace scsi_defs;
using namespace scsi_command_util;

TEST(DiskTest, Dispatch)
{
	MockAbstractController controller(0);
	const unordered_set<uint32_t> sector_sizes;
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	disk->SetRemovable(true);
	disk->MediumChanged();
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdTestUnitReady), scsi_exception);
}

TEST(DiskTest, Rezero)
{
	MockAbstractController controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdRezero), scsi_exception)
		<< "REZERO must fail because drive is not ready";

	disk->SetReady(true);

	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdRezero));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(DiskTest, FormatUnit)
{
	MockAbstractController controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	vector<int>& cmd = controller.GetCmd();

	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdFormat), scsi_exception);

	disk->SetReady(true);

	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdFormat));
	EXPECT_EQ(status::GOOD, controller.GetStatus());

	cmd[1] = 0x10;
	cmd[4] = 1;
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdFormat), scsi_exception);
}

TEST(DiskTest, ReassignBlocks)
{
	MockAbstractController controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdReassign), scsi_exception)
		<< "REASSIGN must fail because drive is not ready";

	disk->SetReady(true);

	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdReassign));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(DiskTest, Seek)
{
	MockAbstractController controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	vector<int>& cmd = controller.GetCmd();

	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdSeek6), scsi_exception)
		<< "SEEK(6) must fail for a medium with 0 blocks";
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdSeek10), scsi_exception)
		<< "SEEK(10) must fail for a medium with 0 blocks";

	disk->SetBlockCount(1);
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdSeek6), scsi_exception)
		<< "SEEK(6) must fail because drive is not ready";
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdSeek10), scsi_exception)
		<< "SEEK(10) must fail because drive is not ready";

	disk->SetReady(true);

	// Block count for SEEK(6)
	cmd[4] = 1;
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdSeek6));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
	cmd[4] = 0;

	// Block count for SEEK(10)
	cmd[5] = 1;
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdSeek10));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(DiskTest, ReadCapacity)
{
	MockAbstractController controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	vector<int>& cmd = controller.GetCmd();

	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16), scsi_exception)
		<< "Neithed READ CAPACITY(16) nor READ LONG(16)";

	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdReadCapacity10), scsi_exception)
		<< "READ CAPACITY(10) must fail because drive is not ready";
	// READ CAPACITY(16), not READ LONG(16)
	cmd[1] = 0x10;
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16), scsi_exception)
		<< "READ CAPACITY(16) must fail because drive is not ready";
	cmd[1] = 0x00;

	disk->SetReady(true);
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdReadCapacity10), scsi_exception)
		<< "READ CAPACITY(10) must fail because the medium has no capacity";
	// READ CAPACITY(16), not READ LONG(16)
	cmd[1] = 0x10;
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16), scsi_exception)
		<< "READ CAPACITY(16) must fail because the medium has no capacity";
	cmd[1] = 0x00;

	disk->SetBlockCount(0x12345678);
	EXPECT_CALL(controller, DataIn());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdReadCapacity10));
	EXPECT_EQ(0x12, controller.GetBuffer()[0]);
	EXPECT_EQ(0x34, controller.GetBuffer()[1]);
	EXPECT_EQ(0x56, controller.GetBuffer()[2]);
	EXPECT_EQ(0x77, controller.GetBuffer()[3]);

	disk->SetBlockCount(0x1234567887654321);
	EXPECT_CALL(controller, DataIn());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdReadCapacity10));
	EXPECT_EQ(0xff, controller.GetBuffer()[0]);
	EXPECT_EQ(0xff, controller.GetBuffer()[1]);
	EXPECT_EQ(0xff, controller.GetBuffer()[2]);
	EXPECT_EQ(0xff, controller.GetBuffer()[3]);

	disk->SetSectorSizeInBytes(1024);
	// READ CAPACITY(16), not READ LONG(16)
	cmd[1] = 0x10;
	EXPECT_CALL(controller, DataIn());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16));
	EXPECT_EQ(0x12, controller.GetBuffer()[0]);
	EXPECT_EQ(0x34, controller.GetBuffer()[1]);
	EXPECT_EQ(0x56, controller.GetBuffer()[2]);
	EXPECT_EQ(0x78, controller.GetBuffer()[3]);
	EXPECT_EQ(0x87, controller.GetBuffer()[4]);
	EXPECT_EQ(0x65, controller.GetBuffer()[5]);
	EXPECT_EQ(0x43, controller.GetBuffer()[6]);
	EXPECT_EQ(0x20, controller.GetBuffer()[7]);
	EXPECT_EQ(0x00, controller.GetBuffer()[8]);
	EXPECT_EQ(0x00, controller.GetBuffer()[9]);
	EXPECT_EQ(0x04, controller.GetBuffer()[10]);
	EXPECT_EQ(0x00, controller.GetBuffer()[11]);
}

TEST(DiskTest, ReadWriteLong)
{
	MockAbstractController controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	vector<int>& cmd = controller.GetCmd();

	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdReadLong10));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdWriteLong10));
	EXPECT_EQ(status::GOOD, controller.GetStatus());

	cmd[2] = 1;
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdReadLong10), scsi_exception)
		<< "READ LONG(10) must fail because the capacity is exceeded";
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdWriteLong10), scsi_exception)
		<< "WRITE LONG(10) must fail because the capacity is exceeded";
	// READ LONG(16), not READ CAPACITY(16)
	cmd[1] = 0x11;
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16), scsi_exception)
		<< "READ LONG(16) must fail because the capacity is exceeded";
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdWriteLong16), scsi_exception)
		<< "WRITE LONG(16) must fail because the capacity is exceeded";
	cmd[2] = 0;

	cmd[7] = 1;
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdReadLong10), scsi_exception)
		<< "READ LONG(10) must fail because it currently only supports 0 bytes transfer length";
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdWriteLong10), scsi_exception)
		<< "WRITE LONG(10) must fail because it currently only supports 0 bytes transfer length";
	cmd[7] = 0;

	// READ LONG(16), not READ CAPACITY(16)
	cmd[1] = 0x11;
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
	cmd[1] = 0x00;
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdWriteLong16));
	EXPECT_EQ(status::GOOD, controller.GetStatus());

	cmd[13] = 1;
	// READ LONG(16), not READ CAPACITY(16)
	cmd[1] = 0x11;
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16), scsi_exception)
		<< "READ LONG(16) must fail because it currently only supports 0 bytes transfer length";
	cmd[1] = 0x00;
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdWriteLong16), scsi_exception)
		<< "WRITE LONG(16) must fail because it currently only supports 0 bytes transfer length";
}

TEST(DiskTest, StartStopUnit)
{
	MockAbstractController controller(0);
	auto disk = make_shared<MockDisk>();
	disk->SetRemovable(true);

	controller.AddDevice(disk);

	vector<int>& cmd = controller.GetCmd();

	// Stop/Unload
	disk->SetReady(true);
	EXPECT_CALL(controller, Status());
	EXPECT_CALL(*disk, FlushCache);
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdStartStop));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
	EXPECT_TRUE(disk->IsStopped());

	// Stop/Load
	cmd[4] = 0x02;
	disk->SetReady(true);
	disk->SetLocked(false);
	EXPECT_CALL(controller, Status());
	EXPECT_CALL(*disk, FlushCache);
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdStartStop));
	EXPECT_EQ(status::GOOD, controller.GetStatus());

	disk->SetReady(false);
	EXPECT_CALL(*disk, FlushCache).Times(0);
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdStartStop), scsi_exception);

	disk->SetReady(true);
	disk->SetLocked(true);
	EXPECT_CALL(*disk, FlushCache).Times(0);
	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdStartStop), scsi_exception);

	// Start/Unload
	cmd[4] = 0x01;
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdStartStop));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
	EXPECT_FALSE(disk->IsStopped());

	// Start/Load
	cmd[4] = 0x03;
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdStartStop));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(DiskTest, PreventAllowMediumRemoval)
{
	MockAbstractController controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	vector<int>& cmd = controller.GetCmd();

	EXPECT_THROW(disk->Dispatch(scsi_command::eCmdRemoval), scsi_exception)
		<< "REMOVAL must fail because drive is not ready";

	disk->SetReady(true);

	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdRemoval));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
	EXPECT_FALSE(disk->IsLocked());

	cmd[4] = 1;
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdRemoval));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
	EXPECT_TRUE(disk->IsLocked());
}

TEST(DiskTest, Eject)
{
	MockDisk disk;

	disk.SetReady(false);
	disk.SetRemovable(false);
	disk.SetLocked(false);
	EXPECT_CALL(disk, FlushCache).Times(0);
	EXPECT_FALSE(disk.Eject(false));

	disk.SetRemovable(true);
	EXPECT_CALL(disk, FlushCache).Times(0);
	EXPECT_FALSE(disk.Eject(false));

	disk.SetReady(true);
	disk.SetLocked(true);
	EXPECT_CALL(disk, FlushCache).Times(0);
	EXPECT_FALSE(disk.Eject(false));

	disk.SetReady(true);
	disk.SetLocked(false);
	EXPECT_CALL(disk, FlushCache);
	EXPECT_TRUE(disk.Eject(false));

	disk.SetReady(true);
	EXPECT_CALL(disk, FlushCache);
	EXPECT_TRUE(disk.Eject(true));
}

TEST(DiskTest, ModeSense6)
{
	NiceMock<MockAbstractController> controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	vector<int>& cmd = controller.GetCmd();

	cmd[2] = 0x3f;
	// ALLOCATION LENGTH
	cmd[4] = 255;
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdModeSense6));
	EXPECT_EQ(0x08, controller.GetBuffer()[3]) << "Wrong block descriptor length";

	cmd[1] = 0x08;
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdModeSense6));
	EXPECT_EQ(0x00, controller.GetBuffer()[2]) << "Wrong device-specific parameter";

	disk->SetReadOnly(false);
	disk->SetProtectable(true);
	disk->SetProtected(true);
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdModeSense6));
	EXPECT_EQ(76, controller.GetBuffer()[0]) << "Wrong data length";
	EXPECT_EQ(0x80, controller.GetBuffer()[2]) << "Wrong device-specific parameter";
}

TEST(DiskTest, ModeSense10)
{
	NiceMock<MockAbstractController> controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	vector<int>& cmd = controller.GetCmd();

	cmd[2] = 0x3f;
	// ALLOCATION LENGTH
	cmd[8] = 255;
	cmd[1] = 0x08;
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdModeSense10));
	EXPECT_EQ(0x00, controller.GetBuffer()[3]) << "Wrong device-specific parameter";

	disk->SetReadOnly(false);
	disk->SetProtectable(true);
	disk->SetProtected(true);
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdModeSense10));
	EXPECT_EQ(80, GetInt16(controller.GetBuffer(), 0)) << "Wrong data length";
	EXPECT_EQ(0x80, controller.GetBuffer()[3]) << "Wrong device-specific parameter";
}

TEST(DiskTest, SynchronizeCache)
{
	MockAbstractController controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	EXPECT_CALL(*disk, FlushCache());
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdSynchronizeCache10));
	EXPECT_EQ(status::GOOD, controller.GetStatus());

	EXPECT_CALL(*disk, FlushCache());
	EXPECT_CALL(controller, Status());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdSynchronizeCache16));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(DiskTest, ReadDefectData)
{
	MockAbstractController controller(0);
	auto disk = make_shared<MockDisk>();

	controller.AddDevice(disk);

	EXPECT_CALL(controller, DataIn());
	EXPECT_TRUE(disk->Dispatch(scsi_command::eCmdReadDefectData10));
	EXPECT_EQ(status::GOOD, controller.GetStatus());
}

TEST(DiskTest, SectorSize)
{
	MockDisk disk;

	unordered_set<uint32_t> sizes = { 512, 1024 };
	disk.SetSectorSizes(sizes);
	EXPECT_TRUE(disk.IsSectorSizeConfigurable());

	sizes.clear();
	disk.SetSectorSizes(sizes);
	EXPECT_FALSE(disk.IsSectorSizeConfigurable());

	disk.SetSectorSizeShiftCount(9);
	EXPECT_EQ(9, disk.GetSectorSizeShiftCount());
	EXPECT_EQ(512, disk.GetSectorSizeInBytes());
	disk.SetSectorSizeInBytes(512);
	EXPECT_EQ(9, disk.GetSectorSizeShiftCount());
	EXPECT_EQ(512, disk.GetSectorSizeInBytes());

	disk.SetSectorSizeShiftCount(10);
	EXPECT_EQ(10, disk.GetSectorSizeShiftCount());
	EXPECT_EQ(1024, disk.GetSectorSizeInBytes());
	disk.SetSectorSizeInBytes(1024);
	EXPECT_EQ(10, disk.GetSectorSizeShiftCount());
	EXPECT_EQ(1024, disk.GetSectorSizeInBytes());

	disk.SetSectorSizeShiftCount(11);
	EXPECT_EQ(11, disk.GetSectorSizeShiftCount());
	EXPECT_EQ(2048, disk.GetSectorSizeInBytes());
	disk.SetSectorSizeInBytes(2048);
	EXPECT_EQ(11, disk.GetSectorSizeShiftCount());
	EXPECT_EQ(2048, disk.GetSectorSizeInBytes());

	disk.SetSectorSizeShiftCount(12);
	EXPECT_EQ(12, disk.GetSectorSizeShiftCount());
	EXPECT_EQ(4096, disk.GetSectorSizeInBytes());
	disk.SetSectorSizeInBytes(4096);
	EXPECT_EQ(12, disk.GetSectorSizeShiftCount());
	EXPECT_EQ(4096, disk.GetSectorSizeInBytes());

	EXPECT_THROW(disk.SetSectorSizeInBytes(1234), io_exception);
}

TEST(DiskTest, ConfiguredSectorSize)
{
	DeviceFactory device_factory;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIHD disk(0, sector_sizes, false);

	EXPECT_TRUE(disk.SetConfiguredSectorSize(device_factory, 512));
	EXPECT_EQ(512, disk.GetConfiguredSectorSize());

	EXPECT_FALSE(disk.SetConfiguredSectorSize(device_factory, 1234));
	EXPECT_EQ(512, disk.GetConfiguredSectorSize());
}

TEST(DiskTest, BlockCount)
{
	MockDisk disk;

	disk.SetBlockCount(0x1234567887654321);
	EXPECT_EQ(0x1234567887654321, disk.GetBlockCount());
}
