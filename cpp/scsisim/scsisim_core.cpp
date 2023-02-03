//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI for Raspberry Pi
//
//	Copyright (C) 2023 akuker
//
//	[ SCSI Bus Emulator ]
//
//---------------------------------------------------------------------------
#include "scsisim_core.h"
#include "shared/log.h"
#include "shared/piscsi_util.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "hal/data_sample_raspberry.h"

#include "scsisim_defs.h"
#include <iostream>
#include <signal.h>
#include <sys/mman.h>

#if defined CONNECT_TYPE_STANDARD
#include "hal/connection_type/connection_standard.h"
#elif defined CONNECT_TYPE_FULLSPEC
#include "hal/connection_type/connection_fullspec.h"
#elif defined CONNECT_TYPE_AIBOM
#include "hal/connection_type/connection_aibom.h"
#elif defined CONNECT_TYPE_GAMERNIUM
#include "hal/connection_type/connection_gamernium.h"
#else
#include "hal/connection_type/connection_gamernium.h"
#endif

using namespace std;
using namespace spdlog;

string current_log_level = "unknown"; // Some versions of spdlog do not support get_log_level()

static ScsiSim* scsi_sim;

void ScsiSim::Banner(const vector<char*>& args) const
{
    cout << piscsi_util::Banner("(SCSI Bus Simulator)");
    cout << "Connection type: " << CONNECT_DESC << '\n' << flush;

    if ((args.size() > 1 && strcmp(args[1], "-h") == 0) || (args.size() > 1 && strcmp(args[1], "--help") == 0)) {
        cout << "\nUsage: " << args[0] << " [-L log_level] ...\n\n";
        exit(EXIT_SUCCESS);
    }
}

bool ScsiSim::SetLogLevel(const string& log_level)
{
    if (log_level == "trace") {
        set_level(level::trace);
        enable_debug = true;
    } else if (log_level == "debug") {
        set_level(level::debug);
        enable_debug = true;
    } else if (log_level == "info") {
        set_level(level::info);
    } else if (log_level == "warn") {
        set_level(level::warn);
    } else if (log_level == "err") {
        set_level(level::err);
    } else if (log_level == "critical") {
        set_level(level::critical);
    } else if (log_level == "off") {
        set_level(level::off);
    } else {
        return false;
    }

    current_log_level = log_level;

    LOGINFO("Set log level to '%s'", current_log_level.c_str())

    return true;
}

void ScsiSim::TerminationHandler(int signum)
{
    (void)signum;
    // scsi_sim->TeardownSharedMemory();
    scsi_sim->running = false;
    // exit(signum);
}

bool ScsiSim::ParseArgument(const vector<char*>& args)
{
    string name;
    string log_level;

    const char* locale = setlocale(LC_MESSAGES, "");
    if (locale == nullptr || !strcmp(locale, "C")) {
        locale = "en";
    }

    opterr = 1;
    int opt;

    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "-L:t")) != -1) {
        switch (opt) {
        case 'L':
            log_level = optarg;
            continue;
        case 't':
            test_mode = true;
            break;
        default:
            return false;
        }

        if (optopt) {
            return false;
        }
    }

    if (!log_level.empty()) {
        SetLogLevel(log_level);
    }

    return true;
}

int ScsiSim::run(const vector<char*>& args)
{
    running = true;
    scsi_sim = this;

    // added setvbuf to override stdout buffering, so logs are written immediately and not when the process exits.
    setvbuf(stdout, nullptr, _IONBF, 0);

    // Output the Banner
    Banner(args);

    // Create a thread-safe stdout logger to process the log messages
    const auto logger = stdout_color_mt("scsisim stdout logger");
    set_level(level::info);
    current_log_level = "info";

    vector<string> error_list;

    if (!ParseArgument(args)) {
        return -1;
    }

    // We just want to run the test client. After we're done, we can
    // return.
    if(test_mode){
        TestClient();
        return EXIT_SUCCESS;
    }

    // Signal handler to detach all devices on a KILL or TERM signal
    struct sigaction termination_handler;
    termination_handler.sa_handler = TerminationHandler;
    sigemptyset(&termination_handler.sa_mask);
    termination_handler.sa_flags = 0;
    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);

    signals = make_unique<SharedMemory>(SHARED_MEM_NAME, true);

    uint32_t prev_value = signals->get();
    int dot_counter = 0;
    while (running) {
        if (enable_debug) {
            uint32_t new_value = signals->get();

            PrintDifferences(DataSample_Raspberry(new_value), DataSample_Raspberry(prev_value));

            // Note, this won't necessarily print ever data change. It will
            // just give an indication of activity
            if (new_value != prev_value) {
                LOGTRACE("%s Value changed to %08X", __PRETTY_FUNCTION__, new_value);
            }
            prev_value = new_value;
            
            if(++dot_counter > 1000){
                printf(".");
                dot_counter = 0;
            } 
        }
        usleep(1000);
    }

    return EXIT_SUCCESS;
}
