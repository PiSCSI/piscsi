//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "controllers/controller_manager.h"
#include "rascsi_version.h"
#include "test_shared.h"
#include <unistd.h>
#include <vector>
#include <sstream>

using namespace std;

shared_ptr<PrimaryDevice> CreateDevice(PbDeviceType type, MockAbstractController& controller)
{
	DeviceFactory device_factory;
	auto bus = make_shared<MockBus>();
	auto controller_manager = make_shared<ControllerManager>(*bus);

	auto device = device_factory.CreateDevice(*controller_manager, type, 0, "");

	controller.AddDevice(device);

	return device;
}

void TestInquiry(PbDeviceType type, device_type t, scsi_level l, scsi_level r, const string& ident,
		int additional_length, bool removable)
{
    NiceMock<MockAbstractController> controller(0);
    auto device = CreateDevice(type, controller);

    vector<int>& cmd = controller.GetCmd();

    // ALLOCATION LENGTH
    cmd[4] = 255;
    EXPECT_CALL(controller, DataIn());
    EXPECT_TRUE(device->Dispatch(scsi_command::eCmdInquiry));
	const vector<BYTE>& buffer = controller.GetBuffer();
	EXPECT_EQ((int)t, buffer[0]);
	EXPECT_EQ(removable ? 0x80: 0x00, buffer[1]);
	EXPECT_EQ((int)l, buffer[2]);
	EXPECT_EQ((int)r, buffer[3]);
	EXPECT_EQ(additional_length, buffer[4]);
	string product_data;
	if (ident.size() == 24) {
		ostringstream s;
		s << ident << setw(2) << setfill('0') << rascsi_major_version << rascsi_minor_version;
		product_data = s.str();
	}
	else {
		product_data = ident;
	}
	EXPECT_TRUE(!memcmp(product_data.c_str(), &buffer[8], 28));
}

int OpenTempFile(string& file)
{
	char filename[] = "/tmp/rascsi_test-XXXXXX"; //NOSONAR mkstemp() requires a modifiable string

	const int fd = mkstemp(filename);

	file = filename;

	return fd;
}

string CreateTempFile(int size)
{
	char filename[] = "/tmp/rascsi_test-XXXXXX"; //NOSONAR mkstemp() requires a modifiable string
	vector<char> data(size);

	const int fd = mkstemp(filename);
	const size_t count = write(fd, data.data(), data.size());
	close(fd);
	EXPECT_EQ(count, data.size());

	return filename;
}
