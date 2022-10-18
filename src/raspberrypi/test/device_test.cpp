//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "rascsi_exceptions.h"
#include "devices/device.h"

TEST(DeviceTest, Properties)
{
	const int LUN = 5;

	MockDevice device(LUN);

	EXPECT_FALSE(device.IsReady());
	device.SetReady(true);
	EXPECT_TRUE(device.IsReady());

	EXPECT_FALSE(device.IsReset());
	device.SetReset(true);
	EXPECT_TRUE(device.IsReset());

	EXPECT_FALSE(device.IsAttn());
	device.SetAttn(true);
	EXPECT_TRUE(device.IsAttn());

	EXPECT_FALSE(device.IsReadOnly());
	device.SetReadOnly(true);
	EXPECT_TRUE(device.IsReadOnly());
	device.SetReadOnly(false);

	EXPECT_FALSE(device.IsProtectable());
	device.SetProtectable(true);
	EXPECT_TRUE(device.IsProtectable());

	EXPECT_FALSE(device.IsProtected());
	device.SetProtected(true);
	EXPECT_TRUE(device.IsProtected());
	device.SetProtected(false);
	device.SetProtectable(false);
	device.SetReadOnly(true);
	device.SetProtected(true);
	EXPECT_FALSE(device.IsProtected()) << "Read-only or not protectable devices cannot be protected";
	device.SetReadOnly(false);
	device.SetProtected(true);
	EXPECT_FALSE(device.IsProtected()) << "Read-only or not protectable devices cannot be protected";

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

	EXPECT_FALSE(device.SupportsParams());
	EXPECT_TRUE(device.SupportsFile());
	device.SupportsParams(true);
	EXPECT_TRUE(device.SupportsParams());
	EXPECT_FALSE(device.SupportsFile());

	EXPECT_EQ(LUN, device.GetLun());
}

TEST(DeviceTest, GetTypeString)
{
	MockDevice schd(SCHD);
	EXPECT_STREQ("SCHD", schd.GetTypeString());

	MockDevice scrm(SCRM);
	EXPECT_STREQ("SCRM", scrm.GetTypeString());

	MockDevice scmo(SCMO);
	EXPECT_STREQ("SCMO", scmo.GetTypeString());

	MockDevice sccd(SCCD);
	EXPECT_STREQ("SCCD", sccd.GetTypeString());

	MockDevice schs(SCHS);
	EXPECT_STREQ("SCHS", schs.GetTypeString());

	MockDevice scbr(SCBR);
	EXPECT_STREQ("SCBR", scbr.GetTypeString());

	MockDevice scdp(SCDP);
	EXPECT_STREQ("SCDP", scdp.GetTypeString());

	MockDevice sclp(SCLP);
	EXPECT_STREQ("SCLP", sclp.GetTypeString());
}

TEST(DeviceTest, Vendor)
{
	MockDevice device(0);

	EXPECT_THROW(device.SetVendor(""), invalid_argument);
	EXPECT_THROW(device.SetVendor("123456789"), invalid_argument);
	device.SetVendor("12345678");
	EXPECT_EQ("12345678", device.GetVendor());
}

TEST(DeviceTest, Product)
{
	MockDevice device(0);

	EXPECT_THROW(device.SetProduct(""), invalid_argument);
	EXPECT_THROW(device.SetProduct("12345678901234567"), invalid_argument);
	device.SetProduct("1234567890123456");
	EXPECT_EQ("1234567890123456", device.GetProduct());
	device.SetProduct("xyz", false);
	EXPECT_EQ("1234567890123456", device.GetProduct()) << "Changing vital product data is not SCSI complient";
}

TEST(DeviceTest, Revision)
{
	MockDevice device(0);

	EXPECT_THROW(device.SetRevision(""), invalid_argument);
	EXPECT_THROW(device.SetRevision("12345"), invalid_argument);
	device.SetRevision("1234");
	EXPECT_EQ("1234", device.GetRevision());
}

TEST(DeviceTest, GetPaddedName)
{
	MockDevice device(0);

	device.SetVendor("V");
	device.SetProduct("P");
	device.SetRevision("R");

	EXPECT_EQ("V       P               R   ", device.GetPaddedName());
}

TEST(DeviceTest, Params)
{
	MockDevice device(0);
	unordered_map<string, string> params;
	params["key"] = "value";

	EXPECT_EQ("", device.GetParam("key"));

	device.SetParams(params);
	EXPECT_EQ("", device.GetParam("key"));

	unordered_map<string, string> default_params;
	default_params["key"] = "value";
	device.SetDefaultParams(default_params);
	EXPECT_EQ("", device.GetParam("key"));

	device.SetParams(params);
	EXPECT_EQ("value", device.GetParam("key"));
}

TEST(DeviceTest, StatusCode)
{
	MockDevice device(0);

	device.SetStatusCode(123);
	EXPECT_EQ(123, device.GetStatusCode());
}

TEST(DeviceTest, Reset)
{
	MockDevice device(0);

	device.SetLocked(true);
	device.SetAttn(true);
	device.SetReset(true);
	device.Reset();
	EXPECT_FALSE(device.IsLocked());
	EXPECT_FALSE(device.IsAttn());
	EXPECT_FALSE(device.IsReset());
}

TEST(DeviceTest, Start)
{
	MockDevice device(0);

	device.SetStopped(true);
	device.SetReady(false);
	EXPECT_FALSE(device.Start());
	EXPECT_TRUE(device.IsStopped());
	device.SetReady(true);
	EXPECT_TRUE(device.Start());
	EXPECT_FALSE(device.IsStopped());
}

TEST(DeviceTest, Stop)
{
	MockDevice device(0);

	device.SetReady(true);
	device.SetAttn(true);
	device.SetStopped(false);
	device.Stop();
	EXPECT_FALSE(device.IsReady());
	EXPECT_FALSE(device.IsAttn());
	EXPECT_TRUE(device.IsStopped());
}

TEST(DeviceTest, Eject)
{
	MockDevice device(0);

	device.SetReady(false);
	device.SetRemovable(false);
	EXPECT_FALSE(device.Eject(false));
	device.SetReady(true);
	EXPECT_FALSE(device.Eject(false));
	device.SetRemovable(true);
	device.SetLocked(true);
	EXPECT_FALSE(device.Eject(false));
	device.SetLocked(false);
	EXPECT_TRUE(device.Eject(false));
	EXPECT_FALSE(device.IsReady());
	EXPECT_FALSE(device.IsAttn());
	EXPECT_TRUE(device.IsRemoved());
	EXPECT_FALSE(device.IsLocked());
	EXPECT_TRUE(device.IsStopped());
}
