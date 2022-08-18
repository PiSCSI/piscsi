#include "../devices/scsihd.h"

#include <gtest/gtest.h>

using namespace std;

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

TEST(ModePagesTest, SCSIHD_AddModePages)
{
	unordered_set<uint32_t> sector_sizes;
	map<int, vector<BYTE>> mode_pages;

	SCSIHDMock hd_device(sector_sizes);
	hd_device.AddModePages(mode_pages, 0x3f, false);
	EXPECT_EQ(mode_pages.size(), 5);
}

TEST(ModePagesTest, ModePageDevice_AddModePages)
{
	unordered_set<uint32_t> sector_sizes;
	DWORD cdb[6];
	BYTE buf[512];

	SCSIHDMock hd_device(sector_sizes);
	cdb[2] = 0x3f;
	// Buffer can hold all mode page data
	EXPECT_EQ(hd_device.AddModePages(cdb, buf, 512), 102);
	// Allocation length is limited
	EXPECT_EQ(hd_device.AddModePages(cdb, buf, 1), 1);
}
