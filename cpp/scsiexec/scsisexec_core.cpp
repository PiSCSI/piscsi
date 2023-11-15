//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsisexec_core.h"
#include "hal/sbc_version.h"
#include "hal/gpiobus_factory.h"
#include "controllers/controller_manager.h"
#include "shared/piscsi_util.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <csignal>
#include <cstring>
#include <iostream>

using namespace std;
using namespace filesystem;
using namespace spdlog;
using namespace scsi_defs;
using namespace piscsi_util;

void ScsiExec::CleanUp() const
{
    if (bus != nullptr) {
        bus->Cleanup();
    }
}

void ScsiExec::TerminationHandler(int)
{
    instance->bus->SetRST(true);

    instance->CleanUp();

    // Process will terminate automatically
}

bool ScsiExec::Banner(span<char*> args) const
{
    cout << piscsi_util::Banner("(SCSI Action Execution Tool)");

    if (args.size() < 2 || string(args[1]) == "-h" || string(args[1]) == "--help") {
        cout << "Usage: " << args[0] << " -t ID[:LUN] [-i BID] -f FILE [-L LOG_LEVEL] [-b]\n"
            << " ID is the target device ID (0-" << (ControllerManager::GetScsiIdMax() - 1) << ").\n"
            << " LUN is the optional target device LUN (0-" << (ControllerManager::GetScsiLunMax() - 1) << ")."
            << " Default is 0.\n"
            << " BID is the PiSCSI board ID (0-7). Default is 7.\n"
            << " FILENAME is the protobuf input data path.\n"
            << " LOG_LEVEL is the log level {trace|debug|info|warn|err|off}, default is 'info'.\n"
            << " -b signals that the input file is in binary protobuf format instead of JSON format.\n"
            << flush;

        return false;
    }

    return true;
}

bool ScsiExec::Init(bool)
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

void ScsiExec::ParseArguments(span<char*> args)
{
    optind = 1;
    opterr = 0;
    int opt;
    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "i:f:t:bL:")) != -1) {
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
            binary = true;
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

    if (filename.empty()) {
        throw parser_exception("Missing filename");
    }

    if (target_lun == -1) {
        target_lun = 0;
    }
}

int ScsiExec::run(span<char*> args, bool in_process)
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

    string result;
    const bool status = scsi_executor->Execute(filename, binary, result);
    if (status) {
        cout << result << '\n' << flush;
    }
    else {
        cerr << "Error: " << result << endl;
    }

    CleanUp();

    return status ? EXIT_SUCCESS : EXIT_FAILURE;
}

bool ScsiExec::SetLogLevel() const
{
    const level::level_enum l = level::from_str(log_level);
    // Compensate for spdlog using 'off' for unknown levels
    if (to_string_view(l) != log_level) {
        return false;
    }

    set_level(l);

    return true;
}

