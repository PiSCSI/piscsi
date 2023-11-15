//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/bus.h"
#include "generated/piscsi_interface.pb.h"
#include <ctime>
#include <stdexcept>
#include <span>

using namespace std;
using namespace scsi_defs;

class PhaseExecutor
{
    class phase_exception: public runtime_error
    {
        using runtime_error::runtime_error;
    };

public:

    PhaseExecutor(BUS &b, int id) : bus(b), initiator_id(id)
    {
    }
    ~PhaseExecutor() = default;

    void SetTarget(int, int);
    bool Execute(scsi_command, span<uint8_t>, span<uint8_t>, int, int);

    int GetByteCount() const
    {
        return byte_count;
    }

private:

    bool Dispatch(scsi_command, span<uint8_t>, span<uint8_t>, int, int);

    void Reset() const;

    bool Arbitration() const;
    bool Selection() const;
    void Command(scsi_command, span<uint8_t>) const;
    void Status();
    void DataIn(span<uint8_t>, int);
    void DataOut(span<uint8_t>, int);
    void MsgIn() const;
    void MsgOut() const;

    bool WaitForFree() const;
    bool WaitForBusy() const;

    int GetStatus() const
    {
        return status;
    }

    inline void Sleep(const timespec &ns) const
    {
        nanosleep(&ns, nullptr);
    }

    BUS &bus;

    int initiator_id;

    int target_id = -1;
    int target_lun = -1;

    int status = -1;

    int byte_count = 0;

    // Timeout values see bus.h

    inline static const long BUS_SETTLE_DELAY_NS = 400;
    inline static const timespec BUS_SETTLE_DELAY = { .tv_sec = 0, .tv_nsec = BUS_SETTLE_DELAY_NS };

    inline static const long BUS_CLEAR_DELAY_NS = 800;
    inline static const timespec BUS_CLEAR_DELAY = { .tv_sec = 0, .tv_nsec = BUS_CLEAR_DELAY_NS };

    inline static const long BUS_FREE_DELAY_NS = 800;
    inline static const timespec BUS_FREE_DELAY = { .tv_sec = 0, .tv_nsec = BUS_FREE_DELAY_NS };

    inline static const long DESKEW_DELAY_NS = 45;
    inline static const timespec DESKEW_DELAY = { .tv_sec = 0, .tv_nsec = DESKEW_DELAY_NS };

    inline static const long ARBITRATION_DELAY_NS = 2'400;
    inline static const timespec ARBITRATION_DELAY = { .tv_sec = 0, .tv_nsec = ARBITRATION_DELAY_NS };
};
