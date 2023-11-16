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
#include <array>
#include <fstream>
#include <filesystem>

using namespace std;
using namespace filesystem;
using namespace google::protobuf::util;
using namespace scsi_defs;
using namespace piscsi_interface;

string ScsiExecutor::Execute(const string& filename, bool binary, PbResult& result)
{
    int length = 0;

    if (binary) {
        ifstream in(filename, ios::binary);
        if (in.fail()) {
            return "Can't open binary input file '" + filename + "': " + strerror(errno);
        }

        length = file_size(filename);
        vector<char> b(length);
        in.read(b.data(), length);
        memcpy(buffer.data(), b.data(), length);
    }
    else {
        ifstream in(filename);
        if (in.fail()) {
            return "Can't open JSON input file '" + filename + "': " + strerror(errno);
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
        return "Can't execute operation";
    }

    cdb[7] = static_cast<uint8_t>(buffer.size() >> 8);
    cdb[8] = static_cast<uint8_t>(buffer.size());

    if (!phase_executor->Execute(scsi_command::eCmdReadOperationResult, cdb, buffer, buffer.size())) {
        return "Can't read operation result";
     }

    if (binary) {
        if (!result.ParseFromArray(buffer.data(), phase_executor->GetByteCount())) {
            return "Can't parse binary protobuf data";
        }
    }
    else {
        const string json((const char*) buffer.data(), phase_executor->GetByteCount());
         if (!JsonStringToMessage(json, &result).ok()) {
             return "Can't parse JSON protobuf data";
         }
    }

    return "";
}

bool ScsiExecutor::ShutDown()
{
    array<uint8_t, 6> cdb = { };

    phase_executor->Execute(scsi_command::eCmdStartStop, cdb, buffer, 0);

    return true;
}
