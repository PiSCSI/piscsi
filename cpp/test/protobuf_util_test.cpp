//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/protobuf_util.h"
#include "generated/piscsi_interface.pb.h"

using namespace piscsi_interface;
using namespace protobuf_util;

void TestSpecialDevice(const string& name)
{
	PbDeviceDefinition device;
	ParseParameters(device, name);
	EXPECT_EQ(name, GetParam(device, "file"));
	EXPECT_EQ("", GetParam(device, "interfaces"));
}

TEST(ProtobufUtil, GetSetParam)
{
	// The implementation is a template, testing one possible T is sufficient
	PbCommand command;
	SetParam(command, "key", "value");
	EXPECT_EQ("value", GetParam(command, "key"));
	EXPECT_EQ("", GetParam(command, "xyz"));
}

TEST(ProtobufUtil, ParseParameters)
{
	PbDeviceDefinition device1;
	ParseParameters(device1, "a=b:c=d:e");
	EXPECT_EQ("b", GetParam(device1, "a"));
	EXPECT_EQ("d", GetParam(device1, "c"));
	EXPECT_EQ("", GetParam(device1, "e"));

	// Old style parameter
	PbDeviceDefinition device2;
	ParseParameters(device2, "a");
	EXPECT_EQ("a", GetParam(device2, "file"));

	TestSpecialDevice("bridge");
	TestSpecialDevice("daynaport");
	TestSpecialDevice("printer");
	TestSpecialDevice("services");
}

TEST(ProtobufUtil, SetPatternParams)
{
	PbCommand command1;
	SetPatternParams(command1, "file");
	EXPECT_EQ("", GetParam(command1, "folder_pattern"));
	EXPECT_EQ("file", GetParam(command1, "file_pattern"));

	PbCommand command2;
	SetPatternParams(command2, ":file");
	EXPECT_EQ("", GetParam(command2, "folder_pattern"));
	EXPECT_EQ("file", GetParam(command2, "file_pattern"));

	PbCommand command3;
	SetPatternParams(command3, "folder:");
	EXPECT_EQ("folder", GetParam(command3, "folder_pattern"));
	EXPECT_EQ("", GetParam(command3, "file_pattern"));

	PbCommand command4;
	SetPatternParams(command4, "folder:file");
	EXPECT_EQ("folder", GetParam(command4, "folder_pattern"));
	EXPECT_EQ("file", GetParam(command4, "file_pattern"));
}

TEST(ProtobufUtil, ListDevices)
{
	vector<PbDevice> devices;

	EXPECT_FALSE(ListDevices(devices).empty());

	PbDevice device;
	device.set_type(SCHD);
	devices.push_back(device);
	device.set_type(SCBR);
	devices.push_back(device);
	device.set_type(SCDP);
	devices.push_back(device);
	device.set_type(SCHS);
	devices.push_back(device);
	device.set_type(SCLP);
	devices.push_back(device);
	const string device_list = ListDevices(devices);
	EXPECT_FALSE(device_list.empty());
	EXPECT_NE(string::npos, device_list.find("X68000 HOST BRIDGE"));
	EXPECT_NE(string::npos, device_list.find("DaynaPort SCSI/Link"));
	EXPECT_NE(string::npos, device_list.find("Host Services"));
	EXPECT_NE(string::npos, device_list.find("SCSI Printer"));
}

TEST(ProtobufUtil, SetProductData)
{
	PbDeviceDefinition device;

	SetProductData(device, "");
	EXPECT_EQ("", device.vendor());
	EXPECT_EQ("", device.product());
	EXPECT_EQ("", device.revision());

	SetProductData(device, "vendor");
	EXPECT_EQ("vendor", device.vendor());
	EXPECT_EQ("", device.product());
	EXPECT_EQ("", device.revision());

	SetProductData(device, "vendor:product");
	EXPECT_EQ("vendor", device.vendor());
	EXPECT_EQ("product", device.product());
	EXPECT_EQ("", device.revision());

	SetProductData(device, "vendor:product:revision");
	EXPECT_EQ("vendor", device.vendor());
	EXPECT_EQ("product", device.product());
	EXPECT_EQ("revision", device.revision());
}

TEST(ProtobufUtil, SetIdAndLun)
{
	PbDeviceDefinition device;

	EXPECT_NE("", SetIdAndLun(device, "", 32));
	EXPECT_EQ("", SetIdAndLun(device, "1", 32));
	EXPECT_EQ(1, device.id());
	EXPECT_EQ("", SetIdAndLun(device, "2:0", 32));
	EXPECT_EQ(2, device.id());
	EXPECT_EQ(0, device.unit());
}
