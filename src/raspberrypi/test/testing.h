//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <gmock/gmock.h>

#include "controllers/controller_manager.h"
#include "devices/device_factory.h"
#include "devices/primary_device.h"
#include "devices/scsihd.h"
#include "devices/scsihd_nec.h"
#include "devices/scsicd.h"
#include "devices/scsimo.h"
#include "devices/host_services.h"

class MockAbstractController final : public AbstractController //NOSONAR Having many fields/methods cannot be avoided
{
	FRIEND_TEST(AbstractControllerTest, Reset);
	FRIEND_TEST(AbstractControllerTest, SetPhase);
	FRIEND_TEST(AbstractControllerTest, ProcessPhase);
	FRIEND_TEST(AbstractControllerTest, DeviceLunLifeCycle);
	FRIEND_TEST(AbstractControllerTest, ExtractInitiatorId);
	FRIEND_TEST(AbstractControllerTest, GetOpcode);
	FRIEND_TEST(AbstractControllerTest, GetLun);
	FRIEND_TEST(AbstractControllerTest, Ctrl);

public:

	MOCK_METHOD(BUS::phase_t, Process, (int), (override));
	MOCK_METHOD(int, GetEffectiveLun, (), (const override));
	MOCK_METHOD(void, Error, (scsi_defs::sense_key, scsi_defs::asc, scsi_defs::status), (override));
	MOCK_METHOD(int, GetInitiatorId, (), (const override));
	MOCK_METHOD(void, Status, (), ());
	MOCK_METHOD(void, DataIn, (), ());
	MOCK_METHOD(void, DataOut, (), ());
	MOCK_METHOD(void, BusFree, (), ());
	MOCK_METHOD(void, Selection, (), ());
	MOCK_METHOD(void, Command, (), ());
	MOCK_METHOD(void, MsgIn, (), ());
	MOCK_METHOD(void, MsgOut, (), ());
	MOCK_METHOD(void, SetByteTransfer, (bool), (override));
	MOCK_METHOD(void, ScheduleShutdown, (rascsi_shutdown_mode), (override));

	explicit MockAbstractController(int target_id) : AbstractController(nullptr, target_id, 32) {}
	~MockAbstractController() override = default;
};

class MockScsiController final : public ScsiController
{
	FRIEND_TEST(PrimaryDeviceTest, TestUnitReady);
	FRIEND_TEST(PrimaryDeviceTest, Inquiry);
	FRIEND_TEST(PrimaryDeviceTest, RequestSense);
	FRIEND_TEST(PrimaryDeviceTest, ReportLuns);
	FRIEND_TEST(PrimaryDeviceTest, UnknownCommand);
	FRIEND_TEST(DiskTest, Rezero);
	FRIEND_TEST(DiskTest, FormatUnit);
	FRIEND_TEST(DiskTest, ReassignBlocks);
	FRIEND_TEST(DiskTest, Seek);
	FRIEND_TEST(DiskTest, ReadCapacity);
	FRIEND_TEST(DiskTest, ReadWriteLong);
	FRIEND_TEST(DiskTest, ReserveRelease);
	FRIEND_TEST(DiskTest, SendDiagnostic);
	FRIEND_TEST(DiskTest, PreventAllowMediumRemoval);
	FRIEND_TEST(DiskTest, SynchronizeCache);
	FRIEND_TEST(DiskTest, ReadDefectData);

public:

	MOCK_METHOD(void, Reset, (), ());
	MOCK_METHOD(void, Status, (), ());
	MOCK_METHOD(void, DataIn, (), ());
	MOCK_METHOD(void, DataOut, (), ());

	using ScsiController::ScsiController;
};

class MockPrimaryDevice final : public PrimaryDevice
{
	FRIEND_TEST(PrimaryDeviceTest, PhaseChange);
	FRIEND_TEST(PrimaryDeviceTest, TestUnitReady);
	FRIEND_TEST(PrimaryDeviceTest, RequestSense);
	FRIEND_TEST(PrimaryDeviceTest, Inquiry);

public:

	MOCK_METHOD(vector<byte>, InquiryInternal, (), (const));

	MockPrimaryDevice() : PrimaryDevice("test") {}
	~MockPrimaryDevice() override = default;
};

class MockModePageDevice final : public ModePageDevice
{
	FRIEND_TEST(ModePagesTest, ModePageDevice_AddModePages);

	MockModePageDevice() : ModePageDevice("test") {}
	~MockModePageDevice() override = default;

	MOCK_METHOD(vector<byte>, InquiryInternal, (), (const));
	MOCK_METHOD(int, ModeSense6, (const vector<int>&, vector<BYTE>&, int), (const override));
	MOCK_METHOD(int, ModeSense10, (const vector<int>&, vector<BYTE>&, int), (const override));

	void SetUpModePages(map<int, vector<byte>>& pages, int page, bool) const override {
		// Return dummy data for other pages than page 0
		if (page) {
			vector<byte> buf(255);
			pages[page] = buf;
		}
	}
};

class MockSCSIHD final : public SCSIHD
{
	FRIEND_TEST(ModePagesTest, SCSIHD_SetUpModePages);

	explicit MockSCSIHD(const unordered_set<uint32_t>& sector_sizes) : SCSIHD(sector_sizes, false) {}
	~MockSCSIHD() override = default;
};

class MockSCSIHD_NEC final : public SCSIHD_NEC //NOSONAR Ignore inheritance hierarchy depth in unit tests
{
	FRIEND_TEST(ModePagesTest, SCSIHD_NEC_SetUpModePages);
	FRIEND_TEST(DiskTest, Rezero);
	FRIEND_TEST(DiskTest, FormatUnit);
	FRIEND_TEST(DiskTest, ReassignBlocks);
	FRIEND_TEST(DiskTest, Seek);
	FRIEND_TEST(DiskTest, ReadCapacity);
	FRIEND_TEST(DiskTest, ReadWriteLong);
	FRIEND_TEST(DiskTest, ReserveRelease);
	FRIEND_TEST(DiskTest, SendDiagnostic);
	FRIEND_TEST(DiskTest, PreventAllowMediumRemoval);
	FRIEND_TEST(DiskTest, SynchronizeCache);
	FRIEND_TEST(DiskTest, ReadDefectData);
	FRIEND_TEST(DiskTest, SectorSize);
	FRIEND_TEST(DiskTest, ConfiguredSectorSize);
	FRIEND_TEST(DiskTest, BlockCount);

	MOCK_METHOD(void, FlushCache, (), (override));

	using SCSIHD_NEC::SCSIHD_NEC;
};

class MockSCSICD final : public SCSICD
{
	FRIEND_TEST(ModePagesTest, SCSICD_SetUpModePages);

	using SCSICD::SCSICD;
};

class MockSCSIMO final : public SCSIMO
{
	FRIEND_TEST(ModePagesTest, SCSIMO_SetUpModePages);

	using SCSIMO::SCSIMO;
};

class MockHostServices final : public HostServices
{
	FRIEND_TEST(ModePagesTest, HostServices_SetUpModePages);

	using HostServices::HostServices;
};
