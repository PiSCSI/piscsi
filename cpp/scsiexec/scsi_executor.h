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
#include <span>
#include <string>

using namespace std;

class ScsiExecutor
{

public:

    ScsiExecutor(BUS &bus, int id)
    {
        phase_executor = make_unique<PhaseExecutor>(bus, id);
    }
    ~ScsiExecutor() = default;

    void Execute(const string&, bool);

    void SetTarget(int id, int lun)
    {
        phase_executor->SetTarget(id, lun);
    }

private:

    vector<uint8_t> buffer = vector<uint8_t>(65536);

    unique_ptr<PhaseExecutor> phase_executor;
};
