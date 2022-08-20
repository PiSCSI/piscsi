#include "../devices/mode_page_device.h"
#include "../devices/scsihd.h"
#include "../devices/scsicd.h"
#include "../devices/scsimo.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace std;

unordered_set<uint32_t> sector_sizes;
map<int, vector<BYTE>> mode_pages;

class MockModePageDevice : public ModePageDevice
{
public:

	MockModePageDevice() : ModePageDevice("test") { }
	~MockModePageDevice() { }

	MOCK_METHOD(vector<BYTE>, Inquiry, (), (const, override));
	MOCK_METHOD(int, ModeSense6, (const DWORD *, BYTE *), (override));
	MOCK_METHOD(int, ModeSense10, (const DWORD *, BYTE *, int), (override));

	void AddModePages(map<int, vector<BYTE>>& pages, int page, bool) const override {
		// Return dummy data for other pages than page 0
		if (page) {
			vector<BYTE> buf(255);
			pages[page] = buf;
		}
	}

	// Make protected methods visible for testing

	int AddModePages(const DWORD *cdb, BYTE *buf, int max_length) {
		return ModePageDevice::AddModePages(cdb, buf, max_length);
	}
};

// TODO Maybe GoogleMock can be used

class MockSCSIHD : public SCSIHD
{
public:

	MockSCSIHD(const unordered_set<uint32_t>& sector_sizes) : SCSIHD(sector_sizes, false) { };
	~MockSCSIHD() { };

	// Make protected methods visible for testing

	void AddModePages(map<int, vector<BYTE>>& pages, int page, bool changeable) const override {
		SCSIHD::AddModePages(pages, page, changeable);
	}
};

class MockSCSICD : public SCSICD
{
public:

	MockSCSICD(const unordered_set<uint32_t>& sector_sizes) : SCSICD(sector_sizes) { };
	~MockSCSICD() { };

	// Make protected methods visible for testing

	void AddModePages(map<int, vector<BYTE>>& pages, int page, bool changeable) const override {
		SCSICD::AddModePages(pages, page, changeable);
	}
};

class MockSCSIMO : public SCSIMO
{
public:

	MockSCSIMO(const unordered_set<uint32_t>& sector_sizes, const unordered_map<uint64_t, Geometry>& geometries)
		: SCSIMO(sector_sizes, geometries) { };
	~MockSCSIMO() { };

	// Make protected methods visible for testing

	void AddModePages(map<int, vector<BYTE>>& pages, int page, bool changeable) const override {
		SCSIMO::AddModePages(pages, page, changeable);
	}
};

TEST(ModePagesTest, ModePageDevice_AddModePages)
{
	DWORD cdb[6];
	BYTE buf[512];

	MockModePageDevice device;
	cdb[2] = 0x3f;

	// Allocation length is limited
	EXPECT_EQ(device.AddModePages(cdb, buf, 1), 1);

	// Non-existing mode page 0
	cdb[2] = 0x00;
	EXPECT_EQ(device.AddModePages(cdb, buf, 12), 0);
}

TEST(ModePagesTest, SCSIHD_AddModePages)
{
	mode_pages.clear();

	MockSCSIHD device(sector_sizes);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(mode_pages.size(), 5);
	EXPECT_EQ(mode_pages[1].size(), 12);
	EXPECT_EQ(mode_pages[3].size(), 24);
	EXPECT_EQ(mode_pages[4].size(), 24);
	EXPECT_EQ(mode_pages[8].size(), 12);
	EXPECT_EQ(mode_pages[48].size(), 30);
}

TEST(ModePagesTest, SCSICD_AddModePages)
{
	mode_pages.clear();

	MockSCSICD device(sector_sizes);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(mode_pages.size(), 6);
	EXPECT_EQ(mode_pages[1].size(), 12);
	EXPECT_EQ(mode_pages[3].size(), 24);
	EXPECT_EQ(mode_pages[4].size(), 24);
	EXPECT_EQ(mode_pages[8].size(), 12);
	EXPECT_EQ(mode_pages[13].size(), 8);
	EXPECT_EQ(mode_pages[14].size(), 16);
}

TEST(ModePagesTest, SCSIMO_AddModePages)
{
	unordered_map<uint64_t, Geometry> geometries;
	mode_pages.clear();

	MockSCSIMO device(sector_sizes, geometries);
	device.AddModePages(mode_pages, 0x3f, false);

	EXPECT_EQ(mode_pages.size(), 6);
	EXPECT_EQ(mode_pages[1].size(), 12);
	EXPECT_EQ(mode_pages[3].size(), 24);
	EXPECT_EQ(mode_pages[4].size(), 24);
	EXPECT_EQ(mode_pages[6].size(), 4);
	EXPECT_EQ(mode_pages[8].size(), 12);
	EXPECT_EQ(mode_pages[32].size(), 12);
}
