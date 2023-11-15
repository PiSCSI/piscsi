//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/bus.h"
#include "scsiexec/scsi_executor.h"
#include <string>
#include <span>
#include <vector>
#include <unordered_map>

using namespace std;

class ScsiExec
{

public:

    ScsiExec() = default;
    ~ScsiExec() = default;

    int run(span<char*>, bool = false);

private:

    bool Banner(span<char*>) const;
    bool Init(bool);
    void ParseArguments(span<char*>);
    long CalculateEffectiveSize(ostream&) const;

    bool SetLogLevel() const;

    void Reset() const;

    void CleanUp() const;
    static void TerminationHandler(int);

    unique_ptr<BUS> bus;

    unique_ptr<ScsiExecutor> scsi_executor;

    vector<uint8_t> buffer;

    int initiator_id = 7;
    int target_id = -1;
    int target_lun = 0;

    string filename;

    bool binary = false;

    string log_level = "info";

    // Required for the termination handler
    static inline ScsiExec *instance;
};
