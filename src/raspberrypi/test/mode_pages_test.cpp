//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Unit tests based on GoogleTest and GoogleMock
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "exceptions.h"
#include "devices/scsihd.h"
#include "devices/scsihd_nec.h"
#include "devices/scsicd.h"
#include "devices/scsimo.h"
#include "devices/host_services.h"

using namespace std;

namespace ModePagesTest
{

unordered_set<uint32_t> sector_sizes;

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

TEST(ModePagesTest, ModePageDevice_AddModePages)
{
	DWORD cdb[6];
	BYTE buf[512];

	MockModePageDevice device;
	cdb[2] = 0x3f;

	EXPECT_EQ(1, device.AddModePages(cdb, buf, 1))  << "Allocation length was not limited";

	cdb[2] = 0x00;
	EXPECT_THROW(device.AddModePages(cdb, buf, 12), scsi_error_exception) << "Data for non-existing code page 0 were returned";
}

TEST(ModePagesTest, SCSIHD_AddModePages)
{
	map<int, vector<BYTE>> mode_pages;

	MockSCSIHD device(sector_sizes);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(5, mode_pages.size()) << "Unexpected number of code pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(30, mode_pages[48].size());
}

TEST(ModePagesTest, SCSIHD_NEC_AddModePages)
{
	map<int, vector<BYTE>> mode_pages;

	MockSCSIHD_NEC device(sector_sizes);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(5, mode_pages.size()) << "Unexpected number of code pages";
	EXPECT_EQ(8, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(20, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(30, mode_pages[48].size());
}

TEST(ModePagesTest, SCSICD_AddModePages)
{
	map<int, vector<BYTE>> mode_pages;

	MockSCSICD device(sector_sizes);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(6, mode_pages.size()) << "Unexpected number of code pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(8, mode_pages[13].size());
	EXPECT_EQ(16, mode_pages[14].size());
}

TEST(ModePagesTest, SCSIMO_AddModePages)
{
	map<int, vector<BYTE>> mode_pages;
	unordered_map<uint64_t, Geometry> geometries;

	MockSCSIMO device(sector_sizes, geometries);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(6, mode_pages.size()) << "Unexpected number of code pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(4, mode_pages[6].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(12, mode_pages[32].size());
}

TEST(ModePagesTest, HostServices_AddModePages)
{
	map<int, vector<BYTE>> mode_pages;

	MockHostServices device;
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(1, mode_pages.size()) << "Unexpected number of code pages";
	EXPECT_EQ(10, mode_pages[32].size());
}

}
