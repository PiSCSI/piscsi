//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2026 Daniel Markstedt <daniel@mindani.net>
//
//	HDD dump utility (Initiator mode/SASI Version)
//
//	SASI IMAGE EXAMPLE
//		X68000
//			10MB(10441728 BS=256 C=40788)
//			20MB(20748288 BS=256 C=81048)
//			40MB(41496576 BS=256 C=162096)
//
//		MZ-2500/MZ-2800 MZ-1F23
//			20MB(22437888 BS=1024 C=21912)
//
//---------------------------------------------------------------------------

#include "sasidump/sasidump_core.h"
#include "hal/gpiobus_factory.h"
#include "hal/systimer.h"
#include "shared/piscsi_exceptions.h"
#include "shared/piscsi_util.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
#include <unistd.h>

using namespace std;
using namespace piscsi_util;

namespace
{
constexpr uint8_t TEST_UNIT_READY   = 0x00;
constexpr uint8_t REQUEST_SENSE     = 0x03;
constexpr uint8_t READ_6            = 0x08;
constexpr uint8_t WRITE_6           = 0x0a;
constexpr uint32_t PHASE_TIMEOUT_US = 3'000'000;

class sasi_dump_exception : public runtime_error
{
    using runtime_error::runtime_error;
};
} // namespace

bool SasiDump::Banner(span<char*> args) const
{
    cout << piscsi_util::Banner("(SASI Hard Disk Dump/Restore Utility)");

    if (args.size() < 2 || string(args[1]) == "-h" || string(args[1]) == "--help") {
        cout << "Usage: " << args[0] << " -i ID [-u UNIT] [-b BLOCK_SIZE] -c COUNT -f FILE [-r]\n"
             << "See the sasidump man page for all supported parameters\n";
        return false;
    }

    return true;
}

void SasiDump::ParseArguments(span<char*> args)
{
    int option;

    opterr = 0;
    while ((option = getopt(static_cast<int>(args.size()), args.data(), "i:u:b:c:f:r")) != -1) {
        switch (option) {
        case 'i':
            if (!GetAsUnsignedInt(optarg, target_id) || target_id > 7) {
                throw parser_exception("Invalid target ID (0-7)");
            }
            break;

        case 'u':
            if (!GetAsUnsignedInt(optarg, unit_id) || unit_id > 1) {
                throw parser_exception("Invalid unit ID (0-1)");
            }
            break;

        case 'b': {
            int value;
            if (!GetAsUnsignedInt(optarg, value) || (value != 256 && value != 512 && value != 1024)) {
                throw parser_exception("Invalid block size (256, 512, or 1024)");
            }
            block_size = static_cast<uint32_t>(value);
        } break;

        case 'c': {
            int value;
            if (!GetAsUnsignedInt(optarg, value) || value == 0 || value > static_cast<int>(MAX_BLOCK_ADDRESS + 1)) {
                throw parser_exception("Invalid block count (1-" + to_string(MAX_BLOCK_ADDRESS + 1) + ")");
            }
            block_count = static_cast<uint32_t>(value);
        } break;

        case 'f':
            filename = optarg;
            break;

        case 'r':
            restore = true;
            break;

        default:
            throw parser_exception("Invalid option");
        }
    }

    if (target_id == -1) {
        throw parser_exception("Missing target ID");
    }
    if (block_count == 0) {
        throw parser_exception("Missing block count");
    }
    if (filename.empty()) {
        throw parser_exception("Missing filename");
    }
}

bool SasiDump::Init() const
{
    struct sigaction termination_handler{};
    termination_handler.sa_handler = TerminationHandler;
    sigemptyset(&termination_handler.sa_mask);
    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGHUP, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);
    signal(SIGPIPE, SIG_IGN);

    bus = GPIOBUS_Factory::Create(BUS::mode_e::INITIATOR);
    return bus != nullptr;
}

void SasiDump::CleanUp()
{
    if (bus != nullptr) {
        bus->Cleanup();
    }
}

void SasiDump::TerminationHandler(int)
{
    CleanUp();
}

void SasiDump::ResetBus() const
{
    bus->SetRST(true);
    this_thread::sleep_for(chrono::milliseconds(1));
    bus->SetRST(false);
}

void SasiDump::WaitForPhase(phase_t phase) const
{
    const uint32_t start = SysTimer::GetTimerLow();
    while (SysTimer::GetTimerLow() - start < PHASE_TIMEOUT_US) {
        bus->Acquire();
        if (bus->GetREQ() && bus->GetPhase() == phase) {
            return;
        }
    }

    throw sasi_dump_exception("Expected " + string(BUS::GetPhaseStrRaw(phase)) + " phase, actual phase is " +
                              string(BUS::GetPhaseStrRaw(bus->GetPhase())));
}

void SasiDump::WaitForBusy() const
{
    for (int attempts = 0; attempts < 10'000; attempts++) {
        this_thread::sleep_for(chrono::microseconds(20));
        bus->Acquire();
        if (bus->GetBSY()) {
            return;
        }
    }

    throw sasi_dump_exception("Selection failed");
}

void SasiDump::SelectTarget() const
{
    // Unlike SCSI, SASI selection puts only the target ID on the data bus.
    bus->SetDAT(static_cast<uint8_t>(1U << target_id));
    bus->SetSEL(true);
    WaitForBusy();
    bus->SetSEL(false);
}

void SasiDump::SendCommand(vector<uint8_t>& command) const
{
    SelectTarget();
    WaitForPhase(phase_t::command);

    if (bus->SendHandShake(command.data(), static_cast<int>(command.size()), BUS::SEND_NO_DELAY) !=
        static_cast<int>(command.size())) {
        BusFree();
        throw sasi_dump_exception("Command transfer failed");
    }
}

void SasiDump::ReadData(int length)
{
    WaitForPhase(phase_t::datain);
    if (bus->ReceiveHandShake(buffer.data(), length) != length) {
        throw sasi_dump_exception("DATA IN transfer failed");
    }
}

void SasiDump::WriteData(int length)
{
    WaitForPhase(phase_t::dataout);
    if (bus->SendHandShake(buffer.data(), length, BUS::SEND_NO_DELAY) != length) {
        throw sasi_dump_exception("DATA OUT transfer failed");
    }
}

uint8_t SasiDump::ReceiveStatus() const
{
    array<uint8_t, 1> status = {};
    WaitForPhase(phase_t::status);
    if (bus->ReceiveHandShake(status.data(), static_cast<int>(status.size())) != static_cast<int>(status.size())) {
        throw sasi_dump_exception("STATUS transfer failed");
    }
    return status[0];
}

void SasiDump::ReceiveMessageIn() const
{
    array<uint8_t, 1> message = {};
    WaitForPhase(phase_t::msgin);
    if (bus->ReceiveHandShake(message.data(), static_cast<int>(message.size())) != static_cast<int>(message.size())) {
        throw sasi_dump_exception("MESSAGE IN transfer failed");
    }
}

void SasiDump::BusFree() const
{
    bus->Reset();
}

void SasiDump::TestUnitReady()
{
    try {
        vector<uint8_t> command(6, 0);
        command[0] = TEST_UNIT_READY;
        command[1] = static_cast<uint8_t>(unit_id << 5);
        SendCommand(command);
        ReceiveStatus();
        ReceiveMessageIn();
    } catch (...) {
        BusFree();
        throw;
    }
    BusFree();
}

void SasiDump::RequestSense()
{
    vector<uint8_t> command(6, 0);
    command[0] = REQUEST_SENSE;
    command[1] = static_cast<uint8_t>(unit_id << 5);
    command[4] = 4;

    try {
        SendCommand(command);
        ReadData(4);
        ReceiveStatus();
        ReceiveMessageIn();
    } catch (...) {
        BusFree();
        throw;
    }
    BusFree();
}

void SasiDump::Read6(uint32_t start_block, uint32_t blocks)
{
    vector<uint8_t> command(6, 0);
    command[0] = READ_6;
    command[1] = static_cast<uint8_t>((unit_id << 5) | ((start_block >> 16) & 0x1f));
    command[2] = static_cast<uint8_t>(start_block >> 8);
    command[3] = static_cast<uint8_t>(start_block);
    command[4] = static_cast<uint8_t>(blocks);

    try {
        SendCommand(command);
        ReadData(static_cast<int>(blocks * block_size));
        ReceiveStatus();
        ReceiveMessageIn();
    } catch (...) {
        BusFree();
        throw;
    }
    BusFree();
}

void SasiDump::Write6(uint32_t start_block, uint32_t blocks)
{
    vector<uint8_t> command(6, 0);
    command[0] = WRITE_6;
    command[1] = static_cast<uint8_t>((unit_id << 5) | ((start_block >> 16) & 0x1f));
    command[2] = static_cast<uint8_t>(start_block >> 8);
    command[3] = static_cast<uint8_t>(start_block);
    command[4] = static_cast<uint8_t>(blocks);

    try {
        SendCommand(command);
        WriteData(static_cast<int>(blocks * block_size));
        ReceiveStatus();
        ReceiveMessageIn();
    } catch (...) {
        BusFree();
        throw;
    }
    BusFree();
}

void SasiDump::DumpRestore()
{
    const uint32_t blocks_per_transfer = static_cast<uint32_t>(buffer.size() / block_size);
    const uint64_t transfer_size       = static_cast<uint64_t>(block_count) * block_size;

    cout << "Target ID:       " << target_id << "\n"
         << "Unit ID:         " << unit_id << "\n"
         << "Number of blocks: " << block_count << "\n"
         << "Block length:    " << block_size << " bytes\n"
         << "Total length:    " << transfer_size << " bytes (" << transfer_size / 1024 / 1024 << " MiB)\n";

    ifstream input;
    ofstream output;
    if (restore) {
        input.open(filename, ios::binary);
        if (!input) {
            throw io_exception("Can't open image file '" + filename + "'");
        }

        const auto file_size = filesystem::file_size(filename);
        cout << "Restore file size: " << file_size << " bytes\n";
        if (file_size < transfer_size) {
            throw io_exception("Restore file is smaller than the specified disk size");
        }
        if (file_size > transfer_size) {
            cout << "Warning: Restore file is larger than the specified disk size\n";
        }
    } else {
        output.open(filename, ios::binary | ios::trunc);
        if (!output) {
            throw io_exception("Can't open image file '" + filename + "'");
        }
    }

    ResetBus();
    TestUnitReady();
    RequestSense();

    cout << (restore ? "Restore progress: " : "Dump progress: ") << flush;
    for (uint32_t start_block = 0; start_block < block_count;) {
        const uint32_t blocks = min(blocks_per_transfer, block_count - start_block);
        const auto bytes      = static_cast<streamsize>(blocks * block_size);

        if (restore) {
            input.read(reinterpret_cast<char*>(buffer.data()), bytes);
            if (input.gcount() != bytes) {
                throw io_exception("Failed to read image file");
            }
            Write6(start_block, blocks);
        } else {
            Read6(start_block, blocks);
            output.write(reinterpret_cast<const char*>(buffer.data()), bytes);
            if (!output) {
                throw io_exception("Failed to write image file");
            }
        }

        start_block += blocks;
        cout << '\r' << (start_block * 100 / block_count) << "% (" << start_block << '/' << block_count << ')' << flush;
    }
    cout << '\n';
}

int SasiDump::run(span<char*> args)
{
    if (!Banner(args)) {
        return EXIT_SUCCESS;
    }

    try {
        ParseArguments(args);
    } catch (const parser_exception& e) {
        cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

#ifndef USE_SEL_EVENT_ENABLE
    cerr << "Error: No PiSCSI hardware support\n";
    return EXIT_FAILURE;
#endif

    if (!Init()) {
        cerr << "Error: Can't initialize bus\n";
        return EXIT_FAILURE;
    }

    try {
        DumpRestore();
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << '\n';
        CleanUp();
        return EXIT_FAILURE;
    }

    CleanUp();
    return EXIT_SUCCESS;
}
