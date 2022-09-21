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

// This global variable is convenient but might cause issues because it is reused by several tests
extern DeviceFactory& device_factory;

class MockAbstractController final : public AbstractController
{
public:

	MOCK_METHOD(BUS::phase_t, Process, (int), (override));
	MOCK_METHOD(int, GetEffectiveLun, (), (const override));
	MOCK_METHOD(void, Error, (scsi_defs::sense_key, scsi_defs::asc, scsi_defs::status), (override));
	MOCK_METHOD(int, GetInitiatorId, (), (const override));
	MOCK_METHOD(void, SetUnit, (int), ());
	MOCK_METHOD(void, Connect, (int, BUS *), ());
	MOCK_METHOD(void, Status, (), ());
	MOCK_METHOD(void, DataIn, (), ());
	MOCK_METHOD(void, DataOut, (), ());
	MOCK_METHOD(void, BusFree, (), ());
	MOCK_METHOD(void, Selection, (), ());
	MOCK_METHOD(void, Command, (), ());
	MOCK_METHOD(void, MsgIn, (), ());
	MOCK_METHOD(void, MsgOut, (), ());
	MOCK_METHOD(void, Send, (), ());
	MOCK_METHOD(bool, XferMsg, (int), ());
	MOCK_METHOD(bool, XferIn, (BYTE *), ());
	MOCK_METHOD(bool, XferOut, (bool), ());
	MOCK_METHOD(void, ReceiveBytes, (), ());
	MOCK_METHOD(void, Execute, (), ());
	MOCK_METHOD(void, FlushUnit, (), ());
	MOCK_METHOD(void, Receive, (), ());
	MOCK_METHOD(bool, HasUnit, (), (const override));
	MOCK_METHOD(int, GetMaxLuns, (), (const override));
	MOCK_METHOD(void, SetByteTransfer, (bool), (override));
	MOCK_METHOD(void, ScheduleShutdown, (rascsi_shutdown_mode), (override));
	MOCK_METHOD(void, SetPhase, (BUS::phase_t), (override));
	MOCK_METHOD(void, Reset, (), (override));

	FRIEND_TEST(AbstractControllerTest, DeviceLunLifeCycle);
	FRIEND_TEST(AbstractControllerTest, ExtractInitiatorId);
	FRIEND_TEST(AbstractControllerTest, GetLun);

	explicit MockAbstractController(int target_id) : AbstractController(nullptr, target_id) {}
	~MockAbstractController() final = default;
};

class MockScsiController final : public ScsiController
{
public:

	MOCK_METHOD(BUS::phase_t, Process, (int), (override));
	MOCK_METHOD(void, Error, (scsi_defs::sense_key, scsi_defs::asc, scsi_defs::status), (override));
	MOCK_METHOD(int, GetInitiatorId, (), (const override));
	MOCK_METHOD(void, SetUnit, (int), ());
	MOCK_METHOD(void, Connect, (int, BUS *), ());
	MOCK_METHOD(void, Status, (), ());
	MOCK_METHOD(void, DataIn, (), ());
	MOCK_METHOD(void, DataOut, (), ());
	MOCK_METHOD(void, BusFree, (), ());
	MOCK_METHOD(void, Selection, (), ());
	MOCK_METHOD(void, Command, (), ());
	MOCK_METHOD(void, MsgIn, (), ());
	MOCK_METHOD(void, MsgOut, (), ());
	MOCK_METHOD(void, Send, (), ());
	MOCK_METHOD(bool, XferMsg, (int), ());
	MOCK_METHOD(bool, XferIn, (BYTE *), ());
	MOCK_METHOD(bool, XferOut, (bool), ());
	MOCK_METHOD(void, ReceiveBytes, (), ());
	MOCK_METHOD(void, Execute, (), ());
	MOCK_METHOD(void, FlushUnit, (), ());
	MOCK_METHOD(void, Receive, (), ());
	MOCK_METHOD(bool, HasUnit, (), (const override));
	MOCK_METHOD(void, SetPhase, (BUS::phase_t), (override));
	MOCK_METHOD(void, Sleep, (), ());

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

	using ScsiController::ScsiController;
};

class MockPrimaryDevice final : public PrimaryDevice
{
public:

	MOCK_METHOD(vector<byte>, InquiryInternal, (), (const));

	MockPrimaryDevice() : PrimaryDevice("test") {}
	~MockPrimaryDevice() final = default;

	// Make protected methods visible for testing

	void SetReady(bool ready) { PrimaryDevice::SetReady(ready); }
	void SetReset(bool reset) { PrimaryDevice::SetReset(reset); }
	void SetAttn(bool attn) { PrimaryDevice::SetAttn(attn); }
	vector<byte> HandleInquiry(device_type type, scsi_level level, bool is_removable) const {
		return PrimaryDevice::HandleInquiry(type, level, is_removable);
	}
};

class MockModePageDevice final : public ModePageDevice
{
public:

	MockModePageDevice() : ModePageDevice("test") {}
	~MockModePageDevice() final = default;

	MOCK_METHOD(vector<byte>, InquiryInternal, (), (const));
	MOCK_METHOD(int, ModeSense6, (const vector<int>&, BYTE *, int), (const override));
	MOCK_METHOD(int, ModeSense10, (const vector<int>&, BYTE *, int), (const override));

	void AddModePages(map<int, vector<byte>>& pages, int page, bool) const override {
		// Return dummy data for other pages than page 0
		if (page) {
			vector<byte> buf(255);
			pages[page] = buf;
		}
	}

	// Make protected method visible for testing
	int AddModePages(const vector<int>& cdb, BYTE *buf, int max_length) const {
		return ModePageDevice::AddModePages(cdb, buf, max_length);
	}
};

class MockSCSIHD final : public SCSIHD
{
	FRIEND_TEST(ModePagesTest, SCSIHD_AddModePages);

	explicit MockSCSIHD(const unordered_set<uint32_t>& sector_sizes) : SCSIHD(sector_sizes, false) {}
	~MockSCSIHD() final = default;
};

class MockSCSIHD_NEC final : public SCSIHD_NEC //NOSONAR Ignore inheritance hierarchy depth in unit tests
{
	FRIEND_TEST(ModePagesTest, SCSIHD_NEC_AddModePages);
	FRIEND_TEST(DiskTest, Rezero);
	FRIEND_TEST(DiskTest, FormatUnit);
	FRIEND_TEST(DiskTest, ReassignBlocks);
	FRIEND_TEST(DiskTest, Seek);
	FRIEND_TEST(DiskTest, ReadCapacity);
	FRIEND_TEST(DiskTest, ReadWriteLong);
	FRIEND_TEST(DiskTest, ReserveRelease);
	FRIEND_TEST(DiskTest, SectorSize);
	FRIEND_TEST(DiskTest, ConfiguredSectorSize);
	FRIEND_TEST(DiskTest, BlockCount);

	MockSCSIHD_NEC() = default;
	~MockSCSIHD_NEC() final = default;
};

class MockSCSICD final : public SCSICD
{
	FRIEND_TEST(ModePagesTest, SCSICD_AddModePages);

	explicit MockSCSICD(const unordered_set<uint32_t>& sector_sizes) : SCSICD(sector_sizes) {}
	~MockSCSICD() final = default;
};

class MockSCSIMO final : public SCSIMO
{
	FRIEND_TEST(ModePagesTest, SCSIMO_AddModePages);

	MockSCSIMO(const unordered_set<uint32_t>& sector_sizes, const unordered_map<uint64_t, Geometry>& geometries)
		: SCSIMO(sector_sizes, geometries) {}
	~MockSCSIMO() final = default;
};

class MockHostServices final : public HostServices
{
	FRIEND_TEST(ModePagesTest, HostServices_AddModePages);

public:

	MockHostServices() : HostServices(&DeviceFactory::instance()) {}
	~MockHostServices() final = default;
};
