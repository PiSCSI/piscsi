//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/scsi.h"
#include "scsiexec/scsi_executor.h"
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

string ScsiExecutor::Execute(const string& filename, bool binary)
{
    int size = 0;

    if (!binary) {
        ifstream in(filename);
        if (in.fail()) {
            return "Error opening JSON input file '" + filename + "'";
        }

        stringstream buf;
        buf << in.rdbuf();
        const string json = buf.str();
        memcpy(buffer.data(), json.data(), json.size());
        size = json.size();
    }
    else {
        ifstream in(filename, ios::binary);
        if (in.fail()) {
            return "Error opening binary input file '" + filename + "'";
        }

        vector<char> b(file_size(filename));
        in.read(b.data(), b.size());
        memcpy(buffer.data(), b.data(), b.size());
        size = b.size();
    }

    vector<uint8_t> cdb(10);
    cdb[1] = binary ? 0x0a : 0x05;
    cdb[5] = static_cast<uint8_t>(size >> 8);
    cdb[6] = static_cast<uint8_t>(size);
    cdb[7] = static_cast<uint8_t>(buffer.size() >> 8);
    cdb[8] = static_cast<uint8_t>(buffer.size());
    phase_executor->Execute(scsi_command::eCmdExecute, cdb, buffer, buffer.size());

    const int length = phase_executor->GetSize();

    if (!binary) {
        const string json((const char*) buffer.data(), length);
        return json;
    }
    else {
        PbResult result;
        if (!result.ParseFromArray(buffer.data(), length)) {
            assert(false);
        }
        string json;
        google::protobuf::util::MessageToJsonString(result, &json);
        return json;
    }
}
