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

// Note that these global variables are convenient,
// but might cause issues because they are reused by all tests
extern DeviceFactory& device_factory;
extern ControllerManager& controller_manager;

class MockBus : public BUS
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
	MOCK_METHOD(bool, GetDP, (), (const, override));
	MOCK_METHOD(DWORD, Acquire,(), (override));
	MOCK_METHOD(int, CommandHandShake, (BYTE *), (override));
	MOCK_METHOD(int, ReceiveHandShake, (BYTE *, int), (override));
	MOCK_METHOD(int, SendHandShake, (BYTE *, int, int), (override));
	MOCK_METHOD(bool, GetSignal, (int), (const, override));
	MOCK_METHOD(void, SetSignal, (int, bool), (override));

	MockBus() {}
	~MockBus() {}
};

class MockAbstractController : public AbstractController
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

	MockAbstractController(int target_id) : AbstractController(nullptr, target_id) {}
	~MockAbstractController() {}
};

class MockScsiController : public ScsiController
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

	FRIEND_TEST(PrimaryDeviceTest, UnitReady);
	FRIEND_TEST(PrimaryDeviceTest, Inquiry);
	FRIEND_TEST(PrimaryDeviceTest, RequestSense);
	FRIEND_TEST(PrimaryDeviceTest, ReportLuns);
	FRIEND_TEST(PrimaryDeviceTest, UnknownCommand);

	MockScsiController(BUS *bus, int target_id) : ScsiController(bus, target_id) {}
	~MockScsiController() {}
};

class MockPrimaryDevice : public PrimaryDevice
{
public:

	MOCK_METHOD(vector<BYTE>, InquiryInternal, (), (const));

	MockPrimaryDevice() : PrimaryDevice("test") {}
	~MockPrimaryDevice() {}

	// Make protected methods visible for testing

	void SetReady(bool ready) { PrimaryDevice::SetReady(ready); }
	void SetReset(bool reset) { PrimaryDevice::SetReset(reset); }
	void SetAttn(bool attn) { PrimaryDevice::SetAttn(attn); }
	vector<BYTE> HandleInquiry(device_type type, scsi_level level, bool is_removable) const {
		return PrimaryDevice::HandleInquiry(type, level, is_removable);
	}
};

class MockModePageDevice : public ModePageDevice
{
public:

	MockModePageDevice() : ModePageDevice("test") { }
	~MockModePageDevice() { }

	MOCK_METHOD(vector<BYTE>, InquiryInternal, (), (const));
	MOCK_METHOD(int, ModeSense6, (const DWORD *, BYTE *, int), ());
	MOCK_METHOD(int, ModeSense10, (const DWORD *, BYTE *, int), ());

	void AddModePages(map<int, vector<BYTE>>& pages, int page, bool) const override {
		// Return dummy data for other pages than page 0
		if (page) {
			vector<BYTE> buf(255);
			pages[page] = buf;
		}
	}

	// Make protected methods visible for testing
	// TODO Why does FRIEND_TEST not work for this method?

	int AddModePages(const DWORD *cdb, BYTE *buf, int max_length) {
		return ModePageDevice::AddModePages(cdb, buf, max_length);
	}
};

class MockSCSIHD : public SCSIHD
{
	FRIEND_TEST(ModePagesTest, SCSIHD_AddModePages);

	MockSCSIHD(const unordered_set<uint32_t>& sector_sizes) : SCSIHD(sector_sizes, false) { };
	~MockSCSIHD() { };
};

class MockSCSIHD_NEC : public SCSIHD_NEC
{
	FRIEND_TEST(ModePagesTest, SCSIHD_NEC_AddModePages);

	MockSCSIHD_NEC(const unordered_set<uint32_t>& sector_sizes) : SCSIHD_NEC(sector_sizes) { };
	~MockSCSIHD_NEC() { };
};

class MockSCSICD : public SCSICD
{
	FRIEND_TEST(ModePagesTest, SCSICD_AddModePages);

	MockSCSICD(const unordered_set<uint32_t>& sector_sizes) : SCSICD(sector_sizes) { };
	~MockSCSICD() { };
};

class MockSCSIMO : public SCSIMO
{
	FRIEND_TEST(ModePagesTest, SCSIMO_AddModePages);

	MockSCSIMO(const unordered_set<uint32_t>& sector_sizes, const unordered_map<uint64_t, Geometry>& geometries)
		: SCSIMO(sector_sizes, geometries) { };
	~MockSCSIMO() { };
};

class MockHostServices : public HostServices
{
	FRIEND_TEST(ModePagesTest, HostServices_AddModePages);

	MockHostServices() { };
	~MockHostServices() { };
};
