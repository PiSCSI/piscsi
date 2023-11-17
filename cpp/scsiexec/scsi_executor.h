//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "scsiexec/phase_executor.h"
#include <cstdint>
#include <array>
#include <string>

using namespace std;
using namespace piscsi_interface;

class ScsiExecutor
{

    // The SCSI Execute command supports a byte count of up to 65535 bytes
    inline static const int BUFFER_SIZE = 65535;

public:

    enum class protobuf_format {
        binary, json,text
    };

    ScsiExecutor(BUS &bus, int id)
    {
        phase_executor = make_unique<PhaseExecutor>(bus, id);
    }
    ~ScsiExecutor() = default;

    string Execute(const string&, protobuf_format, PbResult&);
    bool ShutDown();

    void SetTarget(int id, int lun)
    {
        phase_executor->SetTarget(id, lun);
    }

private:

    array<uint8_t, BUFFER_SIZE> buffer;

    unique_ptr<PhaseExecutor> phase_executor;
};
