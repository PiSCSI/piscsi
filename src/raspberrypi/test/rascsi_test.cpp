#include "../devices/scsihd.h"
#include "../devices/scsicd.h"

#include <gtest/gtest.h>

using namespace std;

// TODO Maybe GoogleMock can be used instead
class SCSIHDMock : public SCSIHD
{
public:

	SCSIHDMock(const unordered_set<uint32_t>& sector_sizes) : SCSIHD(sector_sizes, false) { };
	~SCSIHDMock() { };

	// Make protected methods visible for testing

	int AddModePages(const DWORD *cdb, BYTE *buf, int max_length) {
		return ModePageDevice::AddModePages(cdb, buf, max_length);
	}

	void AddModePages(map<int, vector<BYTE>>& pages, int page, bool changeable) const override
	{
		SCSIHD::AddModePages(pages, page, changeable);
	}
};

class SCSICDMock : public SCSICD
{
public:

	SCSICDMock(const unordered_set<uint32_t>& sector_sizes) : SCSICD(sector_sizes) { };
	~SCSICDMock() { };

	// Make protected methods visible for testing

	int AddModePages(const DWORD *cdb, BYTE *buf, int max_length) {
		return ModePageDevice::AddModePages(cdb, buf, max_length);
	}

	void AddModePages(map<int, vector<BYTE>>& pages, int page, bool changeable) const override
	{
		SCSICD::AddModePages(pages, page, changeable);
	}
};

TEST(ModePagesTest, ModePageDevice_AddModePages)
{
	unordered_set<uint32_t> sector_sizes;
	DWORD cdb[6];
	BYTE buf[512];

	SCSIHDMock hd_device(sector_sizes);
	cdb[2] = 0x3f;
	// Allocation length is limited
	EXPECT_EQ(hd_device.AddModePages(cdb, buf, 1), 1);

	// Non-existing mode page 0
	cdb[2] = 0x00;
	EXPECT_EQ(hd_device.AddModePages(cdb, buf, 12), 0);
}

TEST(ModePagesTest, SCSIHD_AddModePages)
{
	unordered_set<uint32_t> sector_sizes;
	map<int, vector<BYTE>> mode_pages;

	SCSIHDMock hd_device(sector_sizes);
	hd_device.AddModePages(mode_pages, 0x3f, false);
	EXPECT_EQ(mode_pages.size(), 5);
	EXPECT_EQ(mode_pages[1].size(), 12);
	EXPECT_EQ(mode_pages[3].size(), 24);
	EXPECT_EQ(mode_pages[4].size(), 24);
	EXPECT_EQ(mode_pages[8].size(), 12);
	EXPECT_EQ(mode_pages[48].size(), 30);
}

TEST(ModePagesTest, SCSICD_AddModePages)
{
	unordered_set<uint32_t> sector_sizes;
	map<int, vector<BYTE>> mode_pages;

	SCSICDMock cd_device(sector_sizes);
	cd_device.AddModePages(mode_pages, 0x3f, false);
	EXPECT_EQ(mode_pages.size(), 6);
	EXPECT_EQ(mode_pages[1].size(), 12);
	EXPECT_EQ(mode_pages[3].size(), 24);
	EXPECT_EQ(mode_pages[4].size(), 24);
	EXPECT_EQ(mode_pages[8].size(), 12);
	EXPECT_EQ(mode_pages[13].size(), 8);
	EXPECT_EQ(mode_pages[14].size(), 16);
}
