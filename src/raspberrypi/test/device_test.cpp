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
public:

	TestDevice() : Device("test") {}
	~TestDevice() final = default;

	bool Dispatch(scsi_defs::scsi_command) final { return false; }
};

TEST(DeviceTest, Properties)
{
	const int ID = 4;
	const int LUN = 5;

	TestDevice device;

	ASSERT_FALSE(device.IsProtectable());
	device.SetProtectable(true);
	ASSERT_TRUE(device.IsProtectable());

	ASSERT_FALSE(device.IsProtected());
	device.SetProtected(true);
	ASSERT_TRUE(device.IsProtected());

	ASSERT_FALSE(device.IsReadOnly());
	device.SetReadOnly(true);
	ASSERT_TRUE(device.IsReadOnly());

	ASSERT_FALSE(device.IsStoppable());
	device.SetStoppable(true);
	ASSERT_TRUE(device.IsStoppable());

	ASSERT_FALSE(device.IsStopped());
	device.SetStopped(true);
	ASSERT_TRUE(device.IsStopped());

	ASSERT_FALSE(device.IsRemovable());
	device.SetRemovable(true);
	ASSERT_TRUE(device.IsRemovable());

	ASSERT_FALSE(device.IsRemoved());
	device.SetRemoved(true);
	ASSERT_TRUE(device.IsRemoved());

	ASSERT_FALSE(device.IsLockable());
	device.SetLockable(true);
	ASSERT_TRUE(device.IsLockable());

	ASSERT_FALSE(device.IsLocked());
	device.SetLocked(true);
	ASSERT_TRUE(device.IsLocked());

	device.SetId(ID);
	ASSERT_EQ(ID, device.GetId());

	device.SetLun(LUN);
	ASSERT_EQ(LUN, device.GetLun());
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
