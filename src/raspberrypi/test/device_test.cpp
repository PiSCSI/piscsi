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

class TestDevice final : public Device
{
	FRIEND_TEST(DeviceTest, Reset);

public:

	TestDevice() : Device("test") {}
	~TestDevice() override = default;

	bool Dispatch(scsi_defs::scsi_command) override { return false; }
};

TEST(DeviceTest, Reset)
{
	TestDevice device;

	device.SetLocked(true);
	device.SetAttn(true);
	device.SetReset(true);
	device.Reset();
	EXPECT_FALSE(device.IsLocked());
	EXPECT_FALSE(device.IsAttn());
	EXPECT_FALSE(device.IsReset());
}

TEST(DeviceTest, Properties)
{
	const int ID = 4;
	const int LUN = 5;

	TestDevice device;

	EXPECT_FALSE(device.IsProtectable());
	device.SetProtectable(true);
	EXPECT_TRUE(device.IsProtectable());

	EXPECT_FALSE(device.IsProtected());
	device.SetProtected(true);
	EXPECT_TRUE(device.IsProtected());

	EXPECT_FALSE(device.IsReadOnly());
	device.SetReadOnly(true);
	EXPECT_TRUE(device.IsReadOnly());

	EXPECT_FALSE(device.IsStoppable());
	device.SetStoppable(true);
	EXPECT_TRUE(device.IsStoppable());

	EXPECT_FALSE(device.IsStopped());
	device.SetStopped(true);
	EXPECT_TRUE(device.IsStopped());

	EXPECT_FALSE(device.IsRemovable());
	device.SetRemovable(true);
	EXPECT_TRUE(device.IsRemovable());

	EXPECT_FALSE(device.IsRemoved());
	device.SetRemoved(true);
	EXPECT_TRUE(device.IsRemoved());

	EXPECT_FALSE(device.IsLockable());
	device.SetLockable(true);
	EXPECT_TRUE(device.IsLockable());

	EXPECT_FALSE(device.IsLocked());
	device.SetLocked(true);
	EXPECT_TRUE(device.IsLocked());

	device.SetId(ID);
	EXPECT_EQ(ID, device.GetId());

	device.SetLun(LUN);
	EXPECT_EQ(LUN, device.GetLun());
}

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
