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

extern DeviceFactory& device_factory;
extern ControllerManager& controller_manager;

class MockScsiController : public ScsiController
{
public:

	MOCK_METHOD(BUS::phase_t, Process, (int), (override));
	MOCK_METHOD(int, GetEffectiveLun, (), ());
	MOCK_METHOD(void, Error, (scsi_defs::sense_key, scsi_defs::asc, scsi_defs::status), (override));
	MOCK_METHOD(int, GetInitiatorId, (), ());
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

	FRIEND_TEST(PrimaryDeviceTest, TestUnitReady);
	FRIEND_TEST(PrimaryDeviceTest, Inquiry);

	MockScsiController() : ScsiController(nullptr, 0) {}
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
