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
#include "rascsi_exceptions.h"
#include "rascsi_version.h"
#include "test_shared.h"
#include <unistd.h>
#include <vector>
#include <sstream>
#include <filesystem>

using namespace std;
using namespace filesystem;

shared_ptr<PrimaryDevice> CreateDevice(PbDeviceType type, MockAbstractController& controller, const string& extension)
{
	DeviceFactory device_factory;

	auto device = device_factory.CreateDevice(type, 0, extension);
	unordered_map<string, string> params;
	device->Init(params);

	controller.AddDevice(device);

	return device;
}

void TestInquiry(PbDeviceType type, device_type t, scsi_level l, const string& ident, int additional_length,
		bool removable, const string& extension)
{
	auto controller_manager = make_shared<ControllerManager>(make_shared<MockBus>());
	auto controller = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
    auto device = CreateDevice(type, *controller, extension);

    auto& cmd = controller->GetCmd();

    // ALLOCATION LENGTH
    cmd[4] = 255;
    EXPECT_CALL(*controller, DataIn());
    device->Dispatch(scsi_command::eCmdInquiry);
	const vector<uint8_t>& buffer = controller->GetBuffer();
	EXPECT_EQ(t, static_cast<device_type>(buffer[0]));
	EXPECT_EQ(removable ? 0x80: 0x00, buffer[1]);
	EXPECT_EQ(l, static_cast<scsi_level>(buffer[2]));
	EXPECT_EQ(l > scsi_level::SCSI_2 ? scsi_level::SCSI_2 : l, static_cast<scsi_level>(buffer[3]));
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

pair<int, path> OpenTempFile()
{
	const string filename = string(temp_directory_path()) + "/rascsi_test-XXXXXX"; //NOSONAR Publicly writable directory is fine here
	vector<char> f(filename.begin(), filename.end());
	f.push_back(0);

	const int fd = mkstemp(f.data());
	EXPECT_NE(-1, fd) << "Couldn't create temporary file '" << f.data() << "'";

	return make_pair(fd, path(f.data()));
}

path CreateTempFile(int size)
{
	const auto [fd, filename] = OpenTempFile();

	vector<char> data(size);
	const size_t count = write(fd, data.data(), data.size());
	close(fd);
	EXPECT_EQ(count, data.size()) << "Couldn't create temporary file '" << string(filename) << "'";

	return path(filename);
}

int GetInt16(const vector<byte>& buf, int offset)
{
	assert(buf.size() > static_cast<size_t>(offset) + 1);

	return (to_integer<int>(buf[offset]) << 8) | to_integer<int>(buf[offset + 1]);
}

uint32_t GetInt32(const vector<byte>& buf, int offset)
{
	assert(buf.size() > static_cast<size_t>(offset) + 3);

	return (to_integer<uint32_t>(buf[offset]) << 24) | (to_integer<uint32_t>(buf[offset + 1]) << 16) |
			(to_integer<uint32_t>(buf[offset + 2]) << 8) | to_integer<uint32_t>(buf[offset + 3]);
}
