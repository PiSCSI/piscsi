//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/scsi.h"
#include "scsisend/scsi_executor.h"
#include "generated/piscsi_interface.pb.h"
#include <google/protobuf/util/json_util.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

using namespace std;
using namespace filesystem;
using namespace spdlog;
using namespace scsi_defs;
using namespace piscsi_interface;

void ScsiExecutor::Execute(bool binary)
{
    const string file = binary ? "test.bin" : "test.json";

    int size = 0;
    if (!binary) {
        ifstream in(file);
        assert(!in.fail());
        stringstream buf;
        buf << in.rdbuf();
        const string json = buf.str();
        memcpy(buffer.data(), json.data(), json.size());
        size = json.size();
    }
    else {
        ifstream in(file, ios::binary);
        assert(!in.fail());
        vector<char> b(file_size(file));
        in.read(b.data(), b.size());
        memcpy(buffer.data(), b.data(), b.size());
        size = b.size();
    }

    vector<uint8_t> cdb(10);
    cdb[1] = binary ? 0x0a : 0x05;
    cdb[5] = static_cast<uint8_t>(size >> 8);
    cdb[6] = static_cast<uint8_t>(size);
    cdb[7] = static_cast<uint8_t>(65535 >> 8);
    cdb[8] = static_cast<uint8_t>(65535);
    phase_executor->Execute(scsi_command::eCmdExecute, cdb, buffer, 65535);

    const int length = phase_executor->GetSize();

    if (!binary) {
        const string json((const char *)buffer.data(), length);
        cerr << "json received:\n" << json << endl;
    }
    else {
        PbResult result;
        if (!result.ParseFromArray(buffer.data(), length)) {
            assert(false);
        }
        string json;
        google::protobuf::util::MessageToJsonString(result, &json);
        cerr << "json (converted from binary) received:\n" << json << endl;
   }
}
