#include "../devices/scsihd.h"

#include <gtest/gtest.h>

using namespace std;

class SCSIHDMock : public SCSIHD
{
public:

	SCSIHDMock(const unordered_set<uint32_t>& sector_sizes) : SCSIHD(sector_sizes, false) { };
	~SCSIHDMock() { };

	// Make method visible for testing
	void AddModePages(map<int, vector<BYTE>>& pages, int page, bool changeable) const override
	{
		SCSIHD::AddModePages(pages, page, changeable);
	}
};

TEST(ModePageDeviceTest, AddModePages)
{
	unordered_set<uint32_t> sector_sizes;
	map<int, vector<BYTE>> mode_pages;

	SCSIHDMock hd_device(sector_sizes);
	hd_device.AddModePages(mode_pages, 0x3f, false);
	EXPECT_EQ(mode_pages.size(), 5);
}
