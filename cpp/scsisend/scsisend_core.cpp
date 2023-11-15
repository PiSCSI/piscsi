//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsisend/scsisend_core.h"
#include "hal/sbc_version.h"
#include "hal/gpiobus_factory.h"
#include "controllers/controller_manager.h"
#include "shared/piscsi_exceptions.h"
#include "shared/piscsi_util.h"
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <array>

using namespace std;
using namespace filesystem;
using namespace spdlog;
using namespace scsi_defs;
using namespace piscsi_util;

void ScsiSend::CleanUp() const
{
    if (bus != nullptr) {
        bus->Cleanup();
    }
}

void ScsiSend::TerminationHandler(int)
{
    instance->bus->SetRST(true);

    instance->CleanUp();

    // Process will terminate automatically
}

bool ScsiSend::Banner(span<char*> args) const
{
    cout << piscsi_util::Banner("(SCSI Action Trigger Utility)");

    if (args.size() < 2 || string(args[1]) == "-h" || string(args[1]) == "--help") {
        cout << "Usage: " << args[0] << " -t ID[:LUN] [-i BID] [-f FILE] [-a] [-r] [-b BUFFER_SIZE]"
            << " [-L log_level] [-p] [-I] [-s]\n"
            << " ID is the target device ID (0-" << (ControllerManager::GetScsiIdMax() - 1) << ").\n"
            << " LUN is the optional target device LUN (0-" << (ControllerManager::GetScsiLunMax() - 1) << ")."
            << " Default is 0.\n"
            << " BID is the PiSCSI board ID (0-7). Default is 7.\n"
            << " FILE is the protobuf input data path.\n"
            << " BUFFER_SIZE is the transfer buffer size in bytes, at least " << MINIMUM_BUFFER_SIZE
            << " bytes. Default is 1 MiB.\n"
            << flush;

        return false;
    }

    return true;
}

bool ScsiSend::Init(bool in_process)
{
    instance = this;
    // Signal handler for cleaning up
    struct sigaction termination_handler;
    termination_handler.sa_handler = TerminationHandler;
    sigemptyset(&termination_handler.sa_mask);
    termination_handler.sa_flags = 0;
    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);
    signal(SIGPIPE, SIG_IGN);

    bus = GPIOBUS_Factory::Create(BUS::mode_e::INITIATOR);

    if (bus != nullptr) {
        scsi_executor = make_unique<ScsiExecutor>(*bus, initiator_id);
    }

    return bus != nullptr;
}

void ScsiSend::ParseArguments(span<char*> args)
{
    optind = 1;
    opterr = 0;
    int opt;
    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "i:f:b:t:L:arspI")) != -1) {
        switch (opt) {
        case 'i':
            if (!GetAsUnsignedInt(optarg, initiator_id) || initiator_id > 7) {
                throw parser_exception("Invalid PiSCSI board ID " + to_string(initiator_id) + " (0-7)");
            }
            break;

        case 'f':
            filename = optarg;
            break;

        case 'b':
//            if (!GetAsUnsignedInt(optarg, buffer_size) || buffer_size < MINIMUM_BUFFER_SIZE) {
//                throw parser_exception(
//                    "Buffer size must be at least " + to_string(MINIMUM_BUFFER_SIZE / 1024) + " KiB");
//            }

            break;

        case 't':
            if (const string error = ProcessId(optarg, target_id, target_lun); !error.empty()) {
                throw parser_exception(error);
            }
            break;

        case 'L':
            log_level = optarg;
            break;

        default:
            break;
        }
    }

    if (target_lun == -1) {
        target_lun = 0;
    }

    scsi_executor->Execute(false);
}

int ScsiSend::run(span<char*> args, bool in_process)
{
    if (!Banner(args)) {
        return EXIT_SUCCESS;
    }

    try {
        ParseArguments(args);
    }
    catch (const parser_exception &e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    if (target_id == initiator_id) {
        cerr << "Target ID and PiSCSI board ID must not be identical" << endl;
        return EXIT_FAILURE;
    }

    if (!Init(in_process)) {
        cerr << "Error: Can't initialize bus" << endl;
        return EXIT_FAILURE;
    }

    if (!in_process && !SBC_Version::IsRaspberryPi()) {
        cerr << "Error: No PiSCSI hardware support" << endl;
        return EXIT_FAILURE;
    }

    if (!SetLogLevel()) {
        cerr << "Error: Invalid log level '" + log_level + "'";
        return EXIT_FAILURE;
    }

    CleanUp();

    return EXIT_SUCCESS;
}

bool ScsiSend::SetLogLevel() const
{
    const level::level_enum l = level::from_str(log_level);
    // Compensate for spdlog using 'off' for unknown levels
    if (to_string_view(l) != log_level) {
        return false;
    }

    set_level(l);

    return true;
}

