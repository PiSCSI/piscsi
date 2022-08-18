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

private:
	vector<BYTE> inquiry_data;
};

TEST(ModePageDeviceTest, AddModePages)
{
	new ModePageDeviceMock();
}
