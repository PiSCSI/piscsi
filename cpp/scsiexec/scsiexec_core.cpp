//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsiexec_core.h"
#include "hal/sbc_version.h"
#include "hal/gpiobus_factory.h"
#include "controllers/controller_manager.h"
#include "shared/piscsi_util.h"
#include <google/protobuf/util/json_util.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <csignal>
#include <cstring>
#include <iostream>

using namespace std;
using namespace filesystem;
using namespace google::protobuf::util;
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
        cout << "Usage: " << args[0] << " -t ID[:LUN] [-i BID] [-f INPUT_FILE] [-o OUTPUT_FILE]"
            << " -L LOG_LEVEL] [-b] [-X]\n"
            << " ID is the target device ID (0-" << (ControllerManager::GetScsiIdMax() - 1) << ").\n"
            << " LUN is the optional target device LUN (0-" << (ControllerManager::GetScsiLunMax() - 1) << ")."
            << " Default is 0.\n"
            << " BID is the PiSCSI board ID (0-7). Default is 7.\n"
            << " INPUT_FILE is the protobuf data input file.\n"
            << " OUTPUT_FILE is the protobuf data output file. If not specified the output is always JSON"
            << " and goes to stdout.\n"
            << " LOG_LEVEL is the log level {trace|debug|info|warn|err|off}, default is 'info'.\n"
            << " -b Signal that the input file is in binary protobuf format instead of JSON format.\n"
            << " -X Shut down piscsi.\n"
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
    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "i:f:t:bo:L:X")) != -1) {
        switch (opt) {
        case 'i':
            if (!GetAsUnsignedInt(optarg, initiator_id) || initiator_id > 7) {
                throw parser_exception("Invalid PiSCSI board ID " + to_string(initiator_id) + " (0-7)");
            }
            break;

        case 'b':
            binary = true;
            break;

        case 'f':
            input_filename = optarg;
            break;

        case 'o':
            output_filename = optarg;
            if (output_filename.empty()) {
                throw parser_exception("Missing output filename");
            }
           break;

        case 't':
            if (const string error = ProcessId(optarg, target_id, target_lun); !error.empty()) {
                throw parser_exception(error);
            }
            break;

        case 'L':
            log_level = optarg;
            break;

        case 'X':
            shut_down = true;
            break;

        default:
            break;
        }
    }

    if (shut_down) {
        return;
    }

    if (input_filename.empty()) {
        throw parser_exception("Missing input filename");
    }

    if (target_id == -1) {
        throw parser_exception("Missing target ID");
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

    scsi_executor->SetTarget(target_id, target_lun);

    if (shut_down) {
        const bool status = scsi_executor->ShutDown();

        CleanUp();

        return status ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    PbResult result;
    string error;
    const bool status = scsi_executor->Execute(input_filename, binary, result, error);
    if (!status) {
        cerr << "Error: " << error << endl;

        CleanUp();

        return EXIT_FAILURE;
    }

    if (output_filename.empty()) {
        string json;
        MessageToJsonString(result, &json);

        cout << json << '\n' << flush;

        CleanUp();

        return EXIT_SUCCESS;
    }

    // TODO File output

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

