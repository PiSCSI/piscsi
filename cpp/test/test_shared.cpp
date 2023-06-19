//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "test_shared.h"
#include "controllers/controller_manager.h"
#include "mocks.h"
#include "shared/piscsi_exceptions.h"
#include "shared/piscsi_version.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <vector>

using namespace std;
using namespace filesystem;

// Inlude the process id in the temp file path so that multiple instances of the test procedures
// could run on the same host.
const path test_data_temp_path(temp_directory_path() /
                               path(fmt::format("piscsi-test-{}",
                                                getpid()))); // NOSONAR Publicly writable directory is fine here

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
    auto bus                = make_shared<MockBus>();
    auto controller_manager = make_shared<ControllerManager>(*bus);
    auto controller         = make_shared<NiceMock<MockAbstractController>>(controller_manager, 0);
    auto device             = CreateDevice(type, *controller, extension);

    auto& cmd = controller->GetCmd();

    // ALLOCATION LENGTH
    cmd[4] = 255;
    EXPECT_CALL(*controller, DataIn());
    device->Dispatch(scsi_command::eCmdInquiry);
    const vector<uint8_t>& buffer = controller->GetBuffer();
    EXPECT_EQ(t, static_cast<device_type>(buffer[0]));
    EXPECT_EQ(removable ? 0x80 : 0x00, buffer[1]);
    EXPECT_EQ(l, static_cast<scsi_level>(buffer[2]));
    EXPECT_EQ(l > scsi_level::SCSI_2 ? scsi_level::SCSI_2 : l, static_cast<scsi_level>(buffer[3]));
    EXPECT_EQ(additional_length, buffer[4]);
    string product_data;
    if (ident.size() == 24) {
        ostringstream s;
        s << ident << setw(2) << setfill('0') << piscsi_major_version << setw(2) << piscsi_minor_version;
        product_data = s.str();
    } else {
        product_data = ident;
    }
    EXPECT_TRUE(!memcmp(product_data.c_str(), &buffer[8], 28));
}

pair<int, path> OpenTempFile()
{
    const string filename =
        string(test_data_temp_path) + "/piscsi_test-XXXXXX"; // NOSONAR Publicly writable directory is fine here
    vector<char> f(filename.begin(), filename.end());
    f.push_back(0);

    create_directories(path(filename).parent_path());

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

// TODO Replace old-fashinoned C I/O by C++ streams I/O.
// This also avoids potential issues with data type sizes and there is no need for c_str().
void CreateTempFileWithData(const string& filename, vector<uint8_t>& data)
{
    path new_filename = test_data_temp_path;
    new_filename += path(filename);

    create_directories(new_filename.parent_path());

    FILE* fp = fopen(new_filename.c_str(), "wb");
    if (fp == nullptr) {
        cerr << "ERROR: Unable to open file '" << new_filename << "'";
        return;
    }

    if (const size_t size_written = fwrite(&data[0], sizeof(uint8_t), data.size(), fp);
        size_written != sizeof(vector<uint8_t>::value_type) * data.size()) {
    	cerr << "ERROR: Expected to write " << sizeof(vector<uint8_t>::value_type) * data.size() << " bytes"
    			<< ", but only wrote " << data.size() << " to '" << filename << "'";
    }
    fclose(fp);
}

void DeleteTempFile(const string& filename)
{
    path temp_file = test_data_temp_path;
    temp_file += path(filename);
    remove(temp_file);
}

void CleanupAllTempFiles()
{
    remove_all(test_data_temp_path);
}

string ReadTempFileToString(const string& filename)
{
    path temp_file = test_data_temp_path / path(filename);
    ifstream in_fs(temp_file);
    stringstream buffer;
    buffer << in_fs.rdbuf();

    return buffer.str();
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
