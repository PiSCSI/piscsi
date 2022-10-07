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

#include "test_shared.h"
#include "controllers/scsi_controller.h"
#include "devices/primary_device.h"
#include "devices/scsihd.h"
#include "devices/scsihd_nec.h"
#include "devices/scsicd.h"
#include "devices/scsimo.h"
#include "devices/host_services.h"
#include "rascsi/command_context.h"

using namespace testing;

class MockBus : public BUS //NOSONAR Having many fields/methods cannot be avoided
{
public:

	MOCK_METHOD(bool, Init, (mode_e), (override));
	MOCK_METHOD(void, Reset, (), (override));
	MOCK_METHOD(void, Cleanup, (), (override));
	MOCK_METHOD(bool, GetBSY, (), (const override));
	MOCK_METHOD(void, SetBSY, (bool), (override));
	MOCK_METHOD(bool, GetSEL, (), (const override));
	MOCK_METHOD(void, SetSEL, (bool), (override));
	MOCK_METHOD(bool, GetATN, (), (const override));
	MOCK_METHOD(void, SetATN, (bool), (override));
	MOCK_METHOD(bool, GetACK, (), (const override));
	MOCK_METHOD(void, SetACK, (bool), (override));
	MOCK_METHOD(bool, GetRST, (), (const override));
	MOCK_METHOD(void, SetRST, (bool), (override));
	MOCK_METHOD(bool, GetMSG, (), (const override));
	MOCK_METHOD(void, SetMSG, (bool), (override));
	MOCK_METHOD(bool, GetCD, (), (const override));
	MOCK_METHOD(void, SetCD, (bool), (override));
	MOCK_METHOD(bool, GetIO, (), (override));
	MOCK_METHOD(void, SetIO, (bool), (override));
	MOCK_METHOD(bool, GetREQ, (), (const override));
	MOCK_METHOD(void, SetREQ, (bool), (override));
	MOCK_METHOD(BYTE, GetDAT, (), (override));
	MOCK_METHOD(void, SetDAT, (BYTE), (override));
	MOCK_METHOD(bool, GetDP, (), (const override));
	MOCK_METHOD(uint32_t, Acquire, (), (override));
	MOCK_METHOD(int, CommandHandShake, (BYTE *), (override));
	MOCK_METHOD(int, ReceiveHandShake, (BYTE *, int), (override));
	MOCK_METHOD(int, SendHandShake, (BYTE *, int, int), (override));
	MOCK_METHOD(bool, GetSignal, (int), (const override));
	MOCK_METHOD(void, SetSignal, (int, bool), (override));

	MockBus() = default;
	~MockBus() override = default;
};

class MockPhaseHandler : public PhaseHandler
{
	FRIEND_TEST(PhaseHandlerTest, Phases);

public:

	MOCK_METHOD(BUS::phase_t, Process, (int), (override));
	MOCK_METHOD(void, Status, (), ());
	MOCK_METHOD(void, DataIn, (), ());
	MOCK_METHOD(void, DataOut, (), ());
	MOCK_METHOD(void, BusFree, (), ());
	MOCK_METHOD(void, Selection, (), ());
	MOCK_METHOD(void, Command, (), ());
	MOCK_METHOD(void, MsgIn, (), ());
	MOCK_METHOD(void, MsgOut, (), ());

	using PhaseHandler::PhaseHandler;
};

class MockAbstractController : public AbstractController //NOSONAR Having many fields/methods cannot be avoided
{
	friend shared_ptr<PrimaryDevice> CreateDevice(rascsi_interface::PbDeviceType, AbstractController&, int);
	friend void TestInquiry(rascsi_interface::PbDeviceType, scsi_defs::device_type, scsi_defs::scsi_level,
			scsi_defs::scsi_level, const std::string&, int, bool);

	FRIEND_TEST(AbstractControllerTest, Reset);
	FRIEND_TEST(AbstractControllerTest, ProcessPhase);
	FRIEND_TEST(AbstractControllerTest, DeviceLunLifeCycle);
	FRIEND_TEST(AbstractControllerTest, ExtractInitiatorId);
	FRIEND_TEST(AbstractControllerTest, GetOpcode);
	FRIEND_TEST(AbstractControllerTest, GetLun);
	FRIEND_TEST(AbstractControllerTest, Length);
	FRIEND_TEST(AbstractControllerTest, Offset);
	FRIEND_TEST(PrimaryDeviceTest, Inquiry);
	FRIEND_TEST(PrimaryDeviceTest, TestUnitReady);
	FRIEND_TEST(PrimaryDeviceTest, RequestSense);
	FRIEND_TEST(PrimaryDeviceTest, ReportLuns);
	FRIEND_TEST(PrimaryDeviceTest, UnknownCommand);
	FRIEND_TEST(ModePageDeviceTest, ModeSense6);
	FRIEND_TEST(ModePageDeviceTest, ModeSense10);
	FRIEND_TEST(ModePageDeviceTest, ModeSelect6);
	FRIEND_TEST(ModePageDeviceTest, ModeSelect10);
	FRIEND_TEST(DiskTest, Dispatch);
	FRIEND_TEST(DiskTest, Rezero);
	FRIEND_TEST(DiskTest, FormatUnit);
	FRIEND_TEST(DiskTest, ReassignBlocks);
	FRIEND_TEST(DiskTest, Seek);
	FRIEND_TEST(DiskTest, ReadCapacity);
	FRIEND_TEST(DiskTest, ReadWriteLong);
	FRIEND_TEST(DiskTest, Reserve);
	FRIEND_TEST(DiskTest, Release);
	FRIEND_TEST(DiskTest, SendDiagnostic);
	FRIEND_TEST(DiskTest, PreventAllowMediumRemoval);
	FRIEND_TEST(DiskTest, SynchronizeCache);
	FRIEND_TEST(DiskTest, ReadDefectData);

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

	explicit MockAbstractController(int target_id) : AbstractController(bus, target_id, 32) {
		AllocateBuffer(512);
	}
	~MockAbstractController() override = default;

	// Make InitCmd() visible for the unit tests
	vector<int>& InitCmd(int size) { return AbstractController::InitCmd(size); }

	MockBus bus;
};

class MockScsiController : public ScsiController
{
	FRIEND_TEST(ScsiControllerTest, RequestSense);
	FRIEND_TEST(PrimaryDeviceTest, RequestSense);

public:

	MOCK_METHOD(void, Reset, (), ());
	MOCK_METHOD(void, Status, (), ());
	MOCK_METHOD(void, DataIn, (), ());
	MOCK_METHOD(void, DataOut, (), ());
	MOCK_METHOD(void, Error, (scsi_defs::sense_key, scsi_defs::asc, scsi_defs::status), (override));

	explicit MockScsiController(int target_id) : ScsiController(bus, target_id) {}
	~MockScsiController() override = default;

	MockBus bus;
};

class MockDevice : public Device
{
	FRIEND_TEST(DeviceTest, Params);
	FRIEND_TEST(DeviceTest, StatusCode);
	FRIEND_TEST(DeviceTest, Reset);
	FRIEND_TEST(DeviceTest, Start);
	FRIEND_TEST(DeviceTest, Stop);
	FRIEND_TEST(DeviceTest, Eject);

public:

	MOCK_METHOD(int, GetId, (), (const));

	explicit MockDevice(int lun) : Device("test", lun) {}
	~MockDevice() override = default;
};

class MockPrimaryDevice : public PrimaryDevice
{
	FRIEND_TEST(PrimaryDeviceTest, PhaseChange);
	FRIEND_TEST(PrimaryDeviceTest, TestUnitReady);
	FRIEND_TEST(PrimaryDeviceTest, RequestSense);
	FRIEND_TEST(PrimaryDeviceTest, Inquiry);
	FRIEND_TEST(ScsiControllerTest, RequestSense);
	FRIEND_TEST(RascsiExecutorTest, ValidationOperationAgainstDevice);

public:

	MOCK_METHOD(void, Reset, (), ());
	MOCK_METHOD(vector<byte>, InquiryInternal, (), (const));

	explicit MockPrimaryDevice(int lun) : PrimaryDevice("test", lun) {}
	~MockPrimaryDevice() override = default;
};

class MockModePageDevice : public ModePageDevice
{
	FRIEND_TEST(ModePageDeviceTest, AddModePages);

public:

	MOCK_METHOD(vector<byte>, InquiryInternal, (), (const));
	MOCK_METHOD(int, ModeSense6, (const vector<int>&, vector<BYTE>&), (const override));
	MOCK_METHOD(int, ModeSense10, (const vector<int>&, vector<BYTE>&), (const override));

	explicit MockModePageDevice() : ModePageDevice("test", 0) {}
	~MockModePageDevice() override = default;

	void SetUpModePages(map<int, vector<byte>>& pages, int page, bool) const override {
		// Return dummy data for other pages than page 0
		if (page) {
			vector<byte> buf(255);
			pages[page] = buf;
		}
	}
};

class MockDisk : public Disk
{
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
	FRIEND_TEST(DiskTest, BlockCount);
	FRIEND_TEST(DiskTest, GetIdsForReservedFile);

public:

	MOCK_METHOD(vector<byte>, InquiryInternal, (), (const));
	MOCK_METHOD(void, FlushCache, (), (override));

	MockDisk() : Disk("test", 0) {}
	~MockDisk() override = default;
};

class MockSCSIHD : public SCSIHD
{
	FRIEND_TEST(DiskTest, ConfiguredSectorSize);
	FRIEND_TEST(ScsiHdTest, SetUpModePages);
	FRIEND_TEST(RascsiExecutorTest, SetSectorSize);

	using SCSIHD::SCSIHD;
};

class MockSCSIHD_NEC : public SCSIHD_NEC //NOSONAR Ignore inheritance hierarchy depth in unit tests
{
	FRIEND_TEST(ScsiHdNecTest, SetUpModePages);

	using SCSIHD_NEC::SCSIHD_NEC;
};

class MockSCSICD : public SCSICD
{
	FRIEND_TEST(ScsiCdTest, SetUpModePages);

	using SCSICD::SCSICD;
};

class MockSCSIMO : public SCSIMO
{
	FRIEND_TEST(ScsiMoTest, SetUpModePages);

	using SCSIMO::SCSIMO;
};

class MockHostServices : public HostServices
{
	FRIEND_TEST(HostServicesTest, SetUpModePages);

	using HostServices::HostServices;
};

class MockCommandContext : public CommandContext
{
public:

	MockCommandContext() {
		SetFd(open("/dev/null", O_WRONLY));
	}
	~MockCommandContext() = default;
};
