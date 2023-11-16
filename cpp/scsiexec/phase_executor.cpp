//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "hal/bus.h"
#include "phase_executor.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <array>
#include <string>
#include <chrono>

using namespace std;
using namespace spdlog;

void PhaseExecutor::Reset() const
{
    bus.SetDAT(0);
    bus.SetBSY(false);
    bus.SetSEL(false);
    bus.SetATN(false);
}

bool PhaseExecutor::Execute(scsi_command cmd, span<uint8_t> cdb, span<uint8_t> buffer, int length)
{
    status = 0;
    byte_count = 0;

    spdlog::trace(
        fmt::format("Executing {0} for target {1}:{2}", command_mapping.find(cmd)->second.second, target_id,
            target_lun));

    if (!Arbitration()) {
        bus.Reset();
        return false;
    }

    if (!Selection()) {
        Reset();
        return false;
    }

    // Timeout 3 s
    auto now = chrono::steady_clock::now();
    while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 3) {
        bus.Acquire();

        if (bus.GetREQ()) {
            try {
                if (Dispatch(cmd, cdb, buffer, length)) {
                    now = chrono::steady_clock::now();
                }
                else {
                    bus.Reset();
                    return !GetStatus();
                }
            }
            catch (const phase_exception &e) {
                cerr << "Error: " << e.what() << endl;
                bus.Reset();
                return false;
            }
        }
    }

    return false;
}

bool PhaseExecutor::Dispatch(scsi_command cmd, span<uint8_t> cdb, span<uint8_t> buffer, int length)
{
    const phase_t phase = bus.GetPhase();

    spdlog::trace(string("Handling ") + BUS::GetPhaseStrRaw(phase) + " phase");

    switch (phase) {
    case phase_t::command:
        Command(cmd, cdb);
        break;

    case phase_t::status:
        Status();
        break;

    case phase_t::datain:
        DataIn(buffer, length);
        break;

    case phase_t::dataout:
        DataOut(buffer, length);
        break;

    case phase_t::msgin:
        MsgIn();
        // Done with this command cycle
        return false;

    case phase_t::msgout:
        MsgOut();
        break;

    default:
        throw phase_exception(string("Ignoring ") + BUS::GetPhaseStrRaw(phase) + " phase");
        break;
    }

    return true;
}

bool PhaseExecutor::Arbitration() const
{
    if (!WaitForFree()) {
        spdlog::trace("Bus is not free");
        return false;
    }

    Sleep(BUS_FREE_DELAY);

    bus.SetDAT(static_cast<uint8_t>(1 << initiator_id));

    bus.SetBSY(true);

    Sleep(ARBITRATION_DELAY);

    if (bus.GetDAT() > (1 << initiator_id)) {
        spdlog::trace(
            fmt::format("Lost ARBITRATION, competing initiator ID is {}", bus.GetDAT() - (1 << initiator_id)));
        return false;
    }

    bus.SetSEL(true);

    Sleep(BUS_CLEAR_DELAY);
    Sleep(BUS_SETTLE_DELAY);

    return true;
}

bool PhaseExecutor::Selection() const
{
    bus.SetDAT(static_cast<uint8_t>((1 << initiator_id) + (1 << target_id)));

    bus.SetSEL(true);

    // Request MESSAGE OUT for IDENTIFY
    bus.SetATN(true);

    Sleep(DESKEW_DELAY);
    Sleep(DESKEW_DELAY);

    bus.SetBSY(false);

    Sleep(BUS_SETTLE_DELAY);

    if (!WaitForBusy()) {
        spdlog::trace("SELECTION failed");
        return false;
    }

    Sleep(DESKEW_DELAY);
    Sleep(DESKEW_DELAY);

    bus.SetSEL(false);

    return true;
}

void PhaseExecutor::Command(scsi_command cmd, span<uint8_t> cdb) const
{
    cdb[0] = static_cast<uint8_t>(cmd);
    if (target_lun < 8) {
        // Encode LUN in the CDB for backwards compatibility with SCSI-1-CCS
        cdb[1] = static_cast<uint8_t>(cdb[1] + (target_lun << 5));
    }

    if (static_cast<int>(cdb.size()) !=
        bus.SendHandShake(cdb.data(), static_cast<int>(cdb.size()), BUS::SEND_NO_DELAY)) {
        throw phase_exception(command_mapping.find(cmd)->second.second + string(" failed"));
    }
}

void PhaseExecutor::Status()
{
    array<uint8_t, 1> buf;

    if (bus.ReceiveHandShake(buf.data(), 1) != 1) {
        throw phase_exception("STATUS failed");
    }

    status = buf[0];
}

void PhaseExecutor::DataIn(span<uint8_t> buffer, int length)
{
    byte_count = bus.ReceiveHandShake(buffer.data(), length);
    if (!byte_count) {
        throw phase_exception("DATA IN failed");
    }
}

void PhaseExecutor::DataOut(span<uint8_t> buffer, int length)
{
    if (bus.SendHandShake(buffer.data(), length, BUS::SEND_NO_DELAY) != length) {
        throw phase_exception("DATA OUT failed");
    }
}

void PhaseExecutor::MsgIn() const
{
    array<uint8_t, 1> buf;

    if (bus.ReceiveHandShake(buf.data(), buf.size()) != buf.size()) {
        throw phase_exception("MESSAGE IN failed");
    }

    if (buf[0]) {
        throw phase_exception("MESSAGE IN did not report COMMAND COMPLETE");
    }
}

void PhaseExecutor::MsgOut() const
{
    array<uint8_t, 1> buf;

    // IDENTIFY
    buf[0] = static_cast<uint8_t>(target_lun | 0x80);

    if (bus.SendHandShake(buf.data(), buf.size(), BUS::SEND_NO_DELAY) != buf.size()) {
        throw phase_exception("MESSAGE OUT for IDENTIFY failed");
    }
}

bool PhaseExecutor::WaitForFree() const
{
    // Wait for up to 2 s
    int count = 10000;
    do {
        // Wait 20 ms
        Sleep( { .tv_sec = 0, .tv_nsec = 20'000 });
        bus.Acquire();
        if (!bus.GetBSY() && !bus.GetSEL()) {
            return true;
        }
    } while (count--);

    return false;
}

bool PhaseExecutor::WaitForBusy() const
{
    // Wait for up to 2 s
    int count = 10000;
    do {
        // Wait 20 ms
        Sleep( { .tv_sec = 0, .tv_nsec = 20'000 });
        bus.Acquire();
        if (bus.GetBSY()) {
            return true;
        }
    } while (count--);

    return false;
}

void PhaseExecutor::SetTarget(int id, int lun)
{
    target_id = id;
    target_lun = lun;
}
