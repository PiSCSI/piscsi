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
#include <vector>
#include <string>

using namespace std;

class ScsiExecutor
{
    // The SCSI Execute command supports a byte count of up to 65535 bytes
    inline static const int BUFFER_SIZE = 65535;

public:

    ScsiExecutor(BUS &bus, int id)
    {
        phase_executor = make_unique<PhaseExecutor>(bus, id);
    }
    ~ScsiExecutor() = default;

    bool Execute(const string&, bool, string&);

    void SetTarget(int id, int lun)
    {
        phase_executor->SetTarget(id, lun);
    }

private:

    vector<uint8_t> buffer = vector<uint8_t>(BUFFER_SIZE);

    unique_ptr<PhaseExecutor> phase_executor;
};
