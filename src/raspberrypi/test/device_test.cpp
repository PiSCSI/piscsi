//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "rascsi_exceptions.h"
#include "devices/device.h"

class TestDevice : public Device
{
public:

	TestDevice() : Device("test") {}
	~TestDevice() final = default;

	bool Dispatch(scsi_defs::scsi_command) final { return false; }
};

TEST(DeviceTest, ProductData)
{
	TestDevice device;

	EXPECT_THROW(device.SetVendor(""), illegal_argument_exception);
	EXPECT_THROW(device.SetVendor("123456789"), illegal_argument_exception);
	device.SetVendor("12345678");
	EXPECT_EQ("12345678", device.GetVendor());

	EXPECT_THROW(device.SetProduct(""), illegal_argument_exception);
	EXPECT_THROW(device.SetProduct("12345678901234567"), illegal_argument_exception);
	device.SetProduct("1234567890123456");
	EXPECT_EQ("1234567890123456", device.GetProduct());

	EXPECT_THROW(device.SetRevision(""), illegal_argument_exception);
	EXPECT_THROW(device.SetRevision("12345"), illegal_argument_exception);
	device.SetRevision("1234");
	EXPECT_EQ("1234", device.GetRevision());

	device.SetVendor("V");
	device.SetProduct("P");
	device.SetRevision("R");

	EXPECT_EQ("V       P               R   ", device.GetPaddedName());
}
