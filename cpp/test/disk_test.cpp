//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/scsi.h"
#include "shared/piscsi_exceptions.h"
#include "devices/disk.h"
#include "devices/scsi_command_util.h"

using namespace scsi_defs;
using namespace scsi_command_util;

TEST(DiskTest, Dispatch)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	disk->SetRemovable(true);
	disk->SetMediumChanged(false);
	disk->SetReady(true);
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdTestUnitReady);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	disk->SetMediumChanged(true);
	EXPECT_CALL(*controller, Error);
	disk->Dispatch(scsi_command::eCmdTestUnitReady);
	EXPECT_FALSE(disk->IsMediumChanged());
}

TEST(DiskTest, Rezero)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdRezero); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::NOT_READY),
			Property(&scsi_exception::get_asc, asc::MEDIUM_NOT_PRESENT))))
		<< "REZERO must fail because drive is not ready";

	disk->SetReady(true);

	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdRezero);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(DiskTest, FormatUnit)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdFormatUnit); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::NOT_READY),
			Property(&scsi_exception::get_asc, asc::MEDIUM_NOT_PRESENT))))
		<< "FORMAT UNIT must fail because drive is not ready";

	disk->SetReady(true);

	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdFormatUnit);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	cmd[1] = 0x10;
	cmd[4] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdFormatUnit); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))));
}

TEST(DiskTest, ReassignBlocks)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdReassignBlocks); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::NOT_READY),
			Property(&scsi_exception::get_asc, asc::MEDIUM_NOT_PRESENT))))
		<< "REASSIGN must fail because drive is not ready";

	disk->SetReady(true);

	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdReassignBlocks);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(DiskTest, Seek6)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdSeek6); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "SEEK(6) must fail for a medium with 0 blocks";

	disk->SetBlockCount(1);
	// Block count
	cmd[4] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdSeek6); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::NOT_READY),
			Property(&scsi_exception::get_asc, asc::MEDIUM_NOT_PRESENT))))
		<< "SEEK(6) must fail because drive is not ready";

	disk->SetReady(true);

	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdSeek6);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(DiskTest, Seek10)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdSeek10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "SEEK(10) must fail for a medium with 0 blocks";

	disk->SetBlockCount(1);
	// Block count
	cmd[5] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdSeek10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::NOT_READY),
			Property(&scsi_exception::get_asc, asc::MEDIUM_NOT_PRESENT))))
		<< "SEEK(10) must fail because drive is not ready";

	disk->SetReady(true);

	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdSeek10);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(DiskTest, ReadCapacity10)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdReadCapacity10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::NOT_READY),
			Property(&scsi_exception::get_asc, asc::MEDIUM_NOT_PRESENT))))
		<< "READ CAPACITY(10) must fail because drive is not ready";

	disk->SetReady(true);
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdReadCapacity10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::MEDIUM_NOT_PRESENT))))
		<< "READ CAPACITY(10) must fail because the medium has no capacity";

	disk->SetBlockCount(0x12345678);
	EXPECT_CALL(*controller, DataIn);
	disk->Dispatch(scsi_command::eCmdReadCapacity10);
	auto& buf = controller->GetBuffer();
	EXPECT_EQ(0x1234, GetInt16(buf, 0));
	EXPECT_EQ(0x5677, GetInt16(buf, 2));

	disk->SetBlockCount(0x1234567887654321);
	EXPECT_CALL(*controller, DataIn);
	disk->Dispatch(scsi_command::eCmdReadCapacity10);
	buf = controller->GetBuffer();
	EXPECT_EQ(0xffff, GetInt16(buf, 0));
	EXPECT_EQ(0xffff, GetInt16(buf, 2));
}

TEST(DiskTest, ReadCapacity16)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	cmd[1] = 0x00;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
		<< "Neither READ CAPACITY(16) nor READ LONG(16)";

	// READ CAPACITY(16), not READ LONG(16)
	cmd[1] = 0x10;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::NOT_READY),
			Property(&scsi_exception::get_asc, asc::MEDIUM_NOT_PRESENT))))
		<< "READ CAPACITY(16) must fail because drive is not ready";

	disk->SetReady(true);
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::MEDIUM_NOT_PRESENT))))
		<< "READ CAPACITY(16) must fail because the medium has no capacity";

	disk->SetBlockCount(0x1234567887654321);
	disk->SetSectorSizeInBytes(1024);
	EXPECT_CALL(*controller, DataIn);
	disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16);
	const auto& buf = controller->GetBuffer();
	EXPECT_EQ(0x1234, GetInt16(buf, 0));
	EXPECT_EQ(0x5678, GetInt16(buf, 2));
	EXPECT_EQ(0x8765, GetInt16(buf, 4));
	EXPECT_EQ(0x4320, GetInt16(buf, 6));
	EXPECT_EQ(0x0000, GetInt16(buf, 8));
	EXPECT_EQ(0x0400, GetInt16(buf, 10));
}

TEST(DiskTest, Read6)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdRead6); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "READ(6) must fail for a medium with 0 blocks";

	// Further testing requires filesystem access
}

TEST(DiskTest, Read10)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdRead10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "READ(10) must fail for a medium with 0 blocks";

	disk->SetBlockCount(1);
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdRead10);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	// Further testing requires filesystem access
}

TEST(DiskTest, Read16)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdRead16); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "READ(16) must fail for a medium with 0 blocks";

	disk->SetBlockCount(1);
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdRead16);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	// Further testing requires filesystem access
}

TEST(DiskTest, Write6)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdWrite6); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "WRITE(6) must fail for a medium with 0 blocks";

	disk->SetBlockCount(1);
	disk->SetReady(true);
	disk->SetProtectable(true);
	disk->SetProtected(true);
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdWrite6); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::DATA_PROTECT),
			Property(&scsi_exception::get_asc, asc::WRITE_PROTECTED))));

	// Further testing requires filesystem access
}

TEST(DiskTest, Write10)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdWrite10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "WRITE(10) must fail for a medium with 0 blocks";

	disk->SetBlockCount(1);
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdWrite10);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	// Further testing requires filesystem access
}

TEST(DiskTest, Write16)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdWrite16); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "WRITE(16) must fail for a medium with 0 blocks";

	disk->SetBlockCount(1);
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdWrite16);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	// Further testing requires filesystem access
}

TEST(DiskTest, Verify10)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdVerify10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "VERIFY(10) must fail for a medium with 0 blocks";

	disk->SetBlockCount(1);
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdVerify10);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	// Further testing requires filesystem access
}

TEST(DiskTest, Verify16)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdVerify16); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "VERIFY(16) must fail for a medium with 0 blocks";

	disk->SetBlockCount(1);
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdVerify16);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	// Further testing requires filesystem access
}

TEST(DiskTest, ReadLong10)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdReadLong10);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	cmd[2] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdReadLong10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "READ LONG(10) must fail because the capacity is exceeded";
	cmd[2] = 0;

	cmd[7] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdReadLong10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
		<< "READ LONG(10) must fail because it currently only supports 0 bytes transfer length";
}

TEST(DiskTest, ReadLong16)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	// READ LONG(16), not READ CAPACITY(16)
	cmd[1] = 0x11;
	cmd[2] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "READ LONG(16) must fail because the capacity is exceeded";
	cmd[2] = 0;

	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	cmd[13] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdReadCapacity16_ReadLong16); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
		<< "READ LONG(16) must fail because it currently only supports 0 bytes transfer length";
}

TEST(DiskTest, WriteLong10)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdWriteLong10);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	cmd[2] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdWriteLong10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "WRITE LONG(10) must fail because the capacity is exceeded";
	cmd[2] = 0;

	cmd[7] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdWriteLong10); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
		<< "WRITE LONG(10) must fail because it currently only supports 0 bytes transfer length";
}

TEST(DiskTest, WriteLong16)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	cmd[2] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdWriteLong16); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LBA_OUT_OF_RANGE))))
		<< "WRITE LONG(16) must fail because the capacity is exceeded";
	cmd[2] = 0;

	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdWriteLong16);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	cmd[13] = 1;
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdWriteLong16); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::INVALID_FIELD_IN_CDB))))
		<< "WRITE LONG(16) must fail because it currently only supports 0 bytes transfer length";
}

TEST(DiskTest, StartStopUnit)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	disk->SetRemovable(true);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	// Stop/Unload
	disk->SetReady(true);
	EXPECT_CALL(*controller, Status);
	EXPECT_CALL(*disk, FlushCache);
	disk->Dispatch(scsi_command::eCmdStartStop);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
	EXPECT_TRUE(disk->IsStopped());

	// Stop/Load
	cmd[4] = 0x02;
	disk->SetReady(true);
	disk->SetLocked(false);
	EXPECT_CALL(*controller, Status);
	EXPECT_CALL(*disk, FlushCache);
	disk->Dispatch(scsi_command::eCmdStartStop);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	disk->SetReady(false);
	EXPECT_CALL(*disk, FlushCache).Times(0);
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdStartStop); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LOAD_OR_EJECT_FAILED))));

	disk->SetReady(true);
	disk->SetLocked(true);
	EXPECT_CALL(*disk, FlushCache).Times(0);
	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdStartStop); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::ILLEGAL_REQUEST),
			Property(&scsi_exception::get_asc, asc::LOAD_OR_EJECT_FAILED))));

	// Start/Unload
	cmd[4] = 0x01;
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdStartStop);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
	EXPECT_FALSE(disk->IsStopped());

	// Start/Load
	cmd[4] = 0x03;
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdStartStop);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(DiskTest, PreventAllowMediumRemoval)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	EXPECT_THAT([&] { disk->Dispatch(scsi_command::eCmdPreventAllowMediumRemoval); }, Throws<scsi_exception>(AllOf(
			Property(&scsi_exception::get_sense_key, sense_key::NOT_READY),
			Property(&scsi_exception::get_asc, asc::MEDIUM_NOT_PRESENT))))
		<< "REMOVAL must fail because drive is not ready";

	disk->SetReady(true);

	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdPreventAllowMediumRemoval);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
	EXPECT_FALSE(disk->IsLocked());

	cmd[4] = 1;
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdPreventAllowMediumRemoval);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
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

void DiskTest_ValidateFormatPage(shared_ptr<AbstractController> controller, int offset)
{
	const auto& buf = controller->GetBuffer();
	EXPECT_EQ(0x08, buf[offset + 3]) << "Wrong number of trackes in one zone";
	EXPECT_EQ(25, GetInt16(buf, offset + 10)) << "Wrong number of sectors per track";
	EXPECT_EQ(1024, GetInt16(buf, offset + 12)) << "Wrong number of bytes per sector";
	EXPECT_EQ(1, GetInt16(buf, offset + 14)) << "Wrong interleave";
	EXPECT_EQ(11, GetInt16(buf, offset + 16)) << "Wrong track skew factor";
	EXPECT_EQ(20, GetInt16(buf, offset + 18)) << "Wrong cylinder skew factor";
	EXPECT_FALSE(buf[offset + 20] & 0x20) << "Wrong removable flag";
	EXPECT_TRUE(buf[offset + 20] & 0x40) << "Wrong hard-sectored flag";
}

void DiskTest_ValidateDrivePage(shared_ptr<AbstractController> controller, int offset)
{
	const auto& buf = controller->GetBuffer();
	EXPECT_EQ(0x17, buf[offset + 2]);
	EXPECT_EQ(0x4d3b, GetInt16(buf, offset + 3));
	EXPECT_EQ(8, buf[offset + 5]) << "Wrong number of heads";
	EXPECT_EQ(7200, GetInt16(buf, offset + 20)) << "Wrong medium rotation rate";
}

void DiskTest_ValidateCachePage(shared_ptr<AbstractController> controller, int offset)
{
	const auto& buf = controller->GetBuffer();
	EXPECT_EQ(0xffff, GetInt16(buf, offset + 4)) << "Wrong pre-fetch transfer length";
	EXPECT_EQ(0xffff, GetInt16(buf, offset + 8)) << "Wrong maximum pre-fetch";
	EXPECT_EQ(0xffff, GetInt16(buf, offset + 10)) << "Wrong maximum pre-fetch ceiling";
}

TEST(DiskTest, ModeSense6)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	// Drive must be ready on order to return all data
	disk->SetReady(true);

	cmd[2] = 0x3f;
	// ALLOCATION LENGTH
	cmd[4] = 255;
	disk->Dispatch(scsi_command::eCmdModeSense6);
	EXPECT_EQ(0x08, controller->GetBuffer()[3]) << "Wrong block descriptor length";

	// No block descriptor
	cmd[1] = 0x08;
	disk->Dispatch(scsi_command::eCmdModeSense6);
	EXPECT_EQ(0x00, controller->GetBuffer()[2]) << "Wrong device-specific parameter";

	disk->SetReadOnly(false);
	disk->SetProtectable(true);
	disk->SetProtected(true);
	disk->Dispatch(scsi_command::eCmdModeSense6);
	const auto& buf = controller->GetBuffer();
	EXPECT_EQ(0x80, buf[2]) << "Wrong device-specific parameter";

	// Return block descriptor
	cmd[1] = 0x00;

	// Format page
	cmd[2] = 3;
	disk->SetSectorSizeInBytes(1024);
	disk->Dispatch(scsi_command::eCmdModeSense6);
	DiskTest_ValidateFormatPage(controller, 12);

	// Rigid disk drive page
	cmd[2] = 4;
	disk->SetBlockCount(0x12345678);
	disk->Dispatch(scsi_command::eCmdModeSense6);
	DiskTest_ValidateDrivePage(controller, 12);

	// Cache page
	cmd[2] = 8;
	disk->Dispatch(scsi_command::eCmdModeSense6);
	DiskTest_ValidateCachePage(controller, 12);
}

TEST(DiskTest, ModeSense10)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	auto& cmd = controller->GetCmd();

	// Drive must be ready on order to return all data
	disk->SetReady(true);

	cmd[2] = 0x3f;
	// ALLOCATION LENGTH
	cmd[8] = 255;
	disk->Dispatch(scsi_command::eCmdModeSense10);
	EXPECT_EQ(0x08, controller->GetBuffer()[7]) << "Wrong block descriptor length";

	// No block descriptor
	cmd[1] = 0x08;
	disk->Dispatch(scsi_command::eCmdModeSense10);
	auto& buf = controller->GetBuffer();
	EXPECT_EQ(0x00, controller->GetBuffer()[3]) << "Wrong device-specific parameter";

	disk->SetReadOnly(false);
	disk->SetProtectable(true);
	disk->SetProtected(true);
	disk->Dispatch(scsi_command::eCmdModeSense10);
	buf = controller->GetBuffer();
	EXPECT_EQ(0x80, buf[3]) << "Wrong device-specific parameter";

	// Return short block descriptor
	cmd[1] = 0x00;
	disk->SetBlockCount(0x1234);
	disk->SetSectorSizeInBytes(1024);
	disk->Dispatch(scsi_command::eCmdModeSense10);
	buf = controller->GetBuffer();
	EXPECT_EQ(0x00, buf[4]) << "Wrong LONGLBA field";
	EXPECT_EQ(0x08, buf[7]) << "Wrong block descriptor length";
	EXPECT_EQ(0x00, GetInt16(buf, 8));
	EXPECT_EQ(0x1234, GetInt16(buf, 10));
	EXPECT_EQ(0x00, GetInt16(buf, 12));
	EXPECT_EQ(1024, GetInt16(buf, 14));

	// Return long block descriptor
	cmd[1] = 0x10;
	disk->SetBlockCount((uint64_t)0xffffffff + 1);
	disk->Dispatch(scsi_command::eCmdModeSense10);
	buf = controller->GetBuffer();
	EXPECT_EQ(0x01, buf[4]) << "Wrong LONGLBA field";
	EXPECT_EQ(0x10, buf[7]) << "Wrong block descriptor length";
	EXPECT_EQ(0x00, GetInt16(buf, 8));
	EXPECT_EQ(0x01, GetInt16(buf, 10));
	EXPECT_EQ(0x00, GetInt16(buf, 12));
	EXPECT_EQ(0x00, GetInt16(buf, 14));
	EXPECT_EQ(0x00, GetInt16(buf, 20));
	EXPECT_EQ(1024, GetInt16(buf, 22));
	cmd[1] = 0x00;

	// Format page
	cmd[2] = 3;
	disk->SetSectorSizeInBytes(1024);
	disk->Dispatch(scsi_command::eCmdModeSense10);
	DiskTest_ValidateFormatPage(controller, 16);

	// Rigid disk drive page
	cmd[2] = 4;
	disk->SetBlockCount(0x12345678);
	disk->Dispatch(scsi_command::eCmdModeSense10);
	DiskTest_ValidateDrivePage(controller, 16);

	// Cache page
	cmd[2] = 8;
	disk->Dispatch(scsi_command::eCmdModeSense10);
	DiskTest_ValidateCachePage(controller, 16);
}

TEST(DiskTest, SynchronizeCache)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_CALL(*disk, FlushCache);
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdSynchronizeCache10);
	EXPECT_EQ(status::GOOD, controller->GetStatus());

	EXPECT_CALL(*disk, FlushCache);
	EXPECT_CALL(*controller, Status);
	disk->Dispatch(scsi_command::eCmdSynchronizeCache16);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
}

TEST(DiskTest, ReadDefectData)
{
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);
	auto controller = make_shared<MockAbstractController>(controller_manager, 0);
	auto disk = make_shared<MockDisk>();
	const unordered_map<string, string> params;
	disk->Init(params);

	controller->AddDevice(disk);

	EXPECT_CALL(*controller, DataIn);
	disk->Dispatch(scsi_command::eCmdReadDefectData10);
	EXPECT_EQ(status::GOOD, controller->GetStatus());
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
