//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "rascsi_exceptions.h"

TEST(DiskTest, Rezero)
{
	MockScsiController controller(nullptr, 0);
	MockSCSIHD_NEC disk;

	controller.AddDevice(&disk);

	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdRezero), scsi_error_exception)
		<< "REZERO must fail because drive is not ready";

	disk.SetReady(true);

	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdRezero));
	EXPECT_EQ(0, controller.ctrl.status);
}

TEST(DiskTest, FormatUnit)
{
	MockScsiController controller(nullptr, 0);
	MockSCSIHD_NEC disk;

	controller.AddDevice(&disk);

	controller.ctrl.cmd.resize(6);

	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdFormat), scsi_error_exception);

	disk.SetReady(true);

	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdFormat));
	EXPECT_EQ(0, controller.ctrl.status);

	controller.ctrl.cmd[1] = 0x10;
	controller.ctrl.cmd[4] = 1;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdFormat), scsi_error_exception);
}

TEST(DiskTest, ReassignBlocks)
{
	MockScsiController controller(nullptr, 0);
	MockSCSIHD_NEC disk;

	controller.AddDevice(&disk);

	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdReassign), scsi_error_exception)
		<< "REASSIGN must fail because drive is not ready";

	disk.SetReady(true);

	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdReassign));
	EXPECT_EQ(0, controller.ctrl.status);
}

TEST(DiskTest, Seek)
{
	MockScsiController controller(nullptr, 0);
	MockSCSIHD_NEC disk;

	controller.AddDevice(&disk);

	controller.ctrl.cmd.resize(10);

	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdSeek6), scsi_error_exception)
		<< "SEEK(6) must fail for a medium with 0 blocks";
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdSeek10), scsi_error_exception)
		<< "SEEK(10) must fail for a medium with 0 blocks";

	disk.SetBlockCount(1);
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdSeek6), scsi_error_exception)
		<< "SEEK(6) must fail because drive is not ready";
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdSeek10), scsi_error_exception)
		<< "SEEK(10) must fail because drive is not ready";

	disk.SetReady(true);

	// Block count for SEEK(6)
	controller.ctrl.cmd[4] = 1;
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdSeek6));
	EXPECT_EQ(0, controller.ctrl.status);
	controller.ctrl.cmd[4] = 0;

	// Block count for SEEK(10)
	controller.ctrl.cmd[5] = 1;
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdSeek10));
	EXPECT_EQ(0, controller.ctrl.status);
}

TEST(DiskTest, ReadCapacity)
{
	MockScsiController controller(nullptr, 0);
	MockSCSIHD_NEC disk;

	controller.AddDevice(&disk);

	controller.ctrl.cmd.resize(16);

	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16), scsi_error_exception)
		<< "Neithed READ CAPACITY(16) nor READ LONG(16)";

	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdReadCapacity10), scsi_error_exception)
		<< "READ CAPACITY(10) must fail because drive is not ready";
	// READ CAPACITY(16), not READ LONG(16)
	controller.ctrl.cmd[1] = 0x10;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16), scsi_error_exception)
		<< "READ CAPACITY(16) must fail because drive is not ready";
	controller.ctrl.cmd[1] = 0x00;

	disk.SetReady(true);
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdReadCapacity10), scsi_error_exception)
		<< "READ CAPACITY(10) must fail because the medium has no capacity";
	// READ CAPACITY(16), not READ LONG(16)
	controller.ctrl.cmd[1] = 0x10;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16), scsi_error_exception)
		<< "READ CAPACITY(16) must fail because the medium has no capacity";
	controller.ctrl.cmd[1] = 0x00;

	disk.SetBlockCount(0x12345678);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdReadCapacity10));
	EXPECT_EQ(0x12, controller.ctrl.buffer[0]);
	EXPECT_EQ(0x34, controller.ctrl.buffer[1]);
	EXPECT_EQ(0x56, controller.ctrl.buffer[2]);
	EXPECT_EQ(0x77, controller.ctrl.buffer[3]);

	disk.SetBlockCount(0x1234567887654321);
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdReadCapacity10));
	EXPECT_EQ(0xff, controller.ctrl.buffer[0]);
	EXPECT_EQ(0xff, controller.ctrl.buffer[1]);
	EXPECT_EQ(0xff, controller.ctrl.buffer[2]);
	EXPECT_EQ(0xff, controller.ctrl.buffer[3]);

	disk.SetSectorSizeInBytes(1024);
	// READ CAPACITY(16), not READ LONG(16)
	controller.ctrl.cmd[1] = 0x10;
	EXPECT_CALL(controller, DataIn()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16));
	EXPECT_EQ(0x12, controller.ctrl.buffer[0]);
	EXPECT_EQ(0x34, controller.ctrl.buffer[1]);
	EXPECT_EQ(0x56, controller.ctrl.buffer[2]);
	EXPECT_EQ(0x78, controller.ctrl.buffer[3]);
	EXPECT_EQ(0x87, controller.ctrl.buffer[4]);
	EXPECT_EQ(0x65, controller.ctrl.buffer[5]);
	EXPECT_EQ(0x43, controller.ctrl.buffer[6]);
	EXPECT_EQ(0x20, controller.ctrl.buffer[7]);
	EXPECT_EQ(0x00, controller.ctrl.buffer[8]);
	EXPECT_EQ(0x00, controller.ctrl.buffer[9]);
	EXPECT_EQ(0x04, controller.ctrl.buffer[10]);
	EXPECT_EQ(0x00, controller.ctrl.buffer[11]);
}

TEST(DiskTest, ReadWriteLong)
{
	MockScsiController controller(nullptr, 0);
	MockSCSIHD_NEC disk;

	controller.AddDevice(&disk);

	controller.ctrl.cmd.resize(16);

	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdReadLong10));
	EXPECT_EQ(0, controller.ctrl.status);
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdWriteLong10));
	EXPECT_EQ(0, controller.ctrl.status);

	controller.ctrl.cmd[2] = 1;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdReadLong10), scsi_error_exception)
		<< "READ LONG(10) must fail because the capacity is exceeded";
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdWriteLong10), scsi_error_exception)
		<< "WRITE LONG(10) must fail because the capacity is exceeded";
	// READ LONG(16), not READ CAPACITY(16)
	controller.ctrl.cmd[1] = 0x11;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16), scsi_error_exception)
		<< "READ LONG(16) must fail because the capacity is exceeded";
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdWriteLong16), scsi_error_exception)
		<< "WRITE LONG(16) must fail because the capacity is exceeded";
	controller.ctrl.cmd[2] = 0;

	controller.ctrl.cmd[7] = 1;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdReadLong10), scsi_error_exception)
		<< "READ LONG(10) must fail because it currently only supports 0 bytes transfer length";
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdWriteLong10), scsi_error_exception)
		<< "WRITE LONG(10) must fail because it currently only supports 0 bytes transfer length";
	controller.ctrl.cmd[7] = 0;

	// READ LONG(16), not READ CAPACITY(16)
	controller.ctrl.cmd[1] = 0x11;
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16));
	EXPECT_EQ(0, controller.ctrl.status);
	controller.ctrl.cmd[1] = 0x00;
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdWriteLong16));
	EXPECT_EQ(0, controller.ctrl.status);

	controller.ctrl.cmd[13] = 1;
	// READ LONG(16), not READ CAPACITY(16)
	controller.ctrl.cmd[1] = 0x11;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16), scsi_error_exception)
		<< "READ LONG(16) must fail because it currently only supports 0 bytes transfer length";
	controller.ctrl.cmd[1] = 0x00;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdWriteLong16), scsi_error_exception)
		<< "WRITE LONG(16) must fail because it currently only supports 0 bytes transfer length";
}

TEST(DiskTest, ReserveRelease)
{
	MockScsiController controller(nullptr, 0);
	MockSCSIHD_NEC disk;

	controller.AddDevice(&disk);

	controller.ctrl.cmd.resize(6);

	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdReserve6));
	EXPECT_EQ(0, controller.ctrl.status);

	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdRelease6));
	EXPECT_EQ(0, controller.ctrl.status);
}

TEST(DiskTest, SendDiagnostic)
{
	MockScsiController controller(nullptr, 0);
	MockSCSIHD_NEC disk;

	controller.AddDevice(&disk);

	controller.ctrl.cmd.resize(6);

	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdSendDiag));
	EXPECT_EQ(0, controller.ctrl.status);

	controller.ctrl.cmd[1] = 0x10;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdSendDiag), scsi_error_exception)
		<< "SEND DIAGNOSTIC must fail because PF bit is not supported";
	controller.ctrl.cmd[1] = 0;

	controller.ctrl.cmd[3] = 1;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdSendDiag), scsi_error_exception)
		<< "SEND DIAGNOSTIC must fail because parameter list is not supported";
	controller.ctrl.cmd[3] = 0;
	controller.ctrl.cmd[4] = 1;
	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdSendDiag), scsi_error_exception)
		<< "SEND DIAGNOSTIC must fail because parameter list is not supported";
}

TEST(DiskTest, PreventAllowMediumRemoval)
{
	MockScsiController controller(nullptr, 0);
	MockSCSIHD_NEC disk;

	controller.AddDevice(&disk);

	controller.ctrl.cmd.resize(6);

	EXPECT_THROW(disk.Dispatch(scsi_command::eCmdRemoval), scsi_error_exception)
		<< "REMOVAL must fail because drive is not ready";

	disk.SetReady(true);

	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdRemoval));
	EXPECT_EQ(0, controller.ctrl.status);
	EXPECT_FALSE(disk.IsLocked());

	controller.ctrl.cmd[4] = 1;
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdRemoval));
	EXPECT_EQ(0, controller.ctrl.status);
	EXPECT_TRUE(disk.IsLocked());
}

TEST(DiskTest, SynchronizeCache)
{
	MockScsiController controller(nullptr, 0);
	MockSCSIHD_NEC disk;

	controller.AddDevice(&disk);

	controller.ctrl.cmd.resize(10);

	EXPECT_CALL(disk, FlushCache()).Times(1);
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdSynchronizeCache10));
	EXPECT_EQ(0, controller.ctrl.status);

	controller.ctrl.cmd.resize(16);

	EXPECT_CALL(disk, FlushCache()).Times(1);
	EXPECT_CALL(controller, Status()).Times(1);
	EXPECT_TRUE(disk.Dispatch(scsi_command::eCmdSynchronizeCache16));
	EXPECT_EQ(0, controller.ctrl.status);
}

TEST(DiskTest, SectorSize)
{
	MockSCSIHD_NEC disk;

	unordered_set<uint32_t> sizes = { 1, 2, 3 };
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
	MockSCSIHD_NEC disk;

	EXPECT_TRUE(disk.SetConfiguredSectorSize(device_factory, 512));
	EXPECT_EQ(512, disk.GetConfiguredSectorSize());

	EXPECT_FALSE(disk.SetConfiguredSectorSize(device_factory, 1234));
	EXPECT_EQ(512, disk.GetConfiguredSectorSize());
}

TEST(DiskTest, BlockCount)
{
	MockSCSIHD_NEC disk;

	disk.SetBlockCount(0x1234567887654321);
	EXPECT_EQ(0x1234567887654321, disk.GetBlockCount());
}
