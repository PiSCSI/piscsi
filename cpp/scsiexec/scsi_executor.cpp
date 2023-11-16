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
#include <google/protobuf/util/json_util.h>
#include <spdlog/spdlog.h>
#include <array>
#include <fstream>
#include <filesystem>

using namespace std;
using namespace filesystem;
using namespace google::protobuf::util;
using namespace spdlog;
using namespace scsi_defs;
using namespace piscsi_interface;

bool ScsiExecutor::Execute(const string& filename, bool binary, PbResult& result, string& error)
{
    int length = 0;

    if (binary) {
        ifstream in(filename, ios::binary);
        if (in.fail()) {
            error = "Can't open binary input file '" + filename + "': " + strerror(errno);
            return false;
        }

        length = file_size(filename);
        vector<char> b(length);
        in.read(b.data(), length);
        memcpy(buffer.data(), b.data(), length);
    }
    else {
        ifstream in(filename);
        if (in.fail()) {
            error = "Can't open JSON input file '" + filename + "': " + strerror(errno);
            return false;
        }

        stringstream buf;
        buf << in.rdbuf();
        const string json = buf.str();
        length = json.size();
        memcpy(buffer.data(), json.data(), length);
    }

    array<uint8_t, 10> cdb = { };
    cdb[1] = binary ? 0x00 : 0x01;
    cdb[7] = static_cast<uint8_t>(length >> 8);
    cdb[8] = static_cast<uint8_t>(length);

    if (!phase_executor->Execute(scsi_command::eCmdExecuteOperation, cdb, buffer, length)) {
        error = "Can't execute operation";
        return false;
    }

    if (!phase_executor->Execute(scsi_command::eCmdReadOperationResult, cdb, buffer, buffer.size())) {
        error = "Can't read operation result";
        return false;
     }

    length = phase_executor->GetByteCount();

    if (binary) {
        if (!result.ParseFromArray(buffer.data(), length)) {
            error = "Can't parse received binary protobuf data";
            return false;
        }
    }
    else {
        const string json((const char*) buffer.data(), length);
         if (!JsonStringToMessage(json, &result).ok()) {
             error = "Can't parse received JSON protobuf data";
             return false;
         }
    }

    return true;
}

bool ScsiExecutor::ShutDown()
{
    array<uint8_t, 6> cdb = { };

    phase_executor->Execute(scsi_command::eCmdStartStop, cdb, buffer, 0);

    return true;
}
