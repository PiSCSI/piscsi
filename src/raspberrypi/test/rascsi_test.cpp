#include "../devices/mode_page_device.h"

#include <gtest/gtest.h>

using namespace std;

class ModePageDeviceMock: public ModePageDevice
{
public:
	ModePageDeviceMock() : ModePageDevice("test")  { };
	~ModePageDeviceMock() { };

	vector<BYTE> Inquiry() const override { return inquiry_data; }
	int ModeSense6(const DWORD *, BYTE *) override { return 0; }
	int ModeSense10(const DWORD *, BYTE *, int) override { return 0; }
	void AddModePages(map<int, vector<BYTE>>&, int, bool) const override { };

	// Required to make protected method accessible
	int AddModePages(const DWORD *cdb, BYTE *buf, int max_length) {
		return ModePageDevice::AddModePages(cdb, buf, max_length);
	}

private:
	vector<BYTE> inquiry_data;
};

TEST(ModePageDeviceTest, AddModePages)
{
	ModePageDeviceMock device;
	DWORD cdb[6];
	BYTE buf[512];

	cdb[2] = 0x3f;
	device.AddModePages(cdb, buf, 512);
}
