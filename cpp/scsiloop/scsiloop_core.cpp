//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI for Raspberry Pi
//  Loopback tester utility
//
//	Copyright (C) 2022 akuker
//
//	[ Loopback tester utility ]
//
//  For more information, see:
//     https://github.com/PiSCSI/piscsi/wiki/Troubleshooting#Loopback_Testing
//
//---------------------------------------------------------------------------

#include "shared/log.h"
#include "shared/piscsi_version.h"
#include "shared/piscsi_util.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "scsiloop/scsiloop_core.h"
#include "scsiloop/scsiloop_cout.h"
#include "scsiloop/scsiloop_gpio.h"
#include "scsiloop/scsiloop_timer.h"

#include <iostream>
#include <signal.h>

#if defined CONNECT_TYPE_STANDARD
#include "hal/connection_type/connection_standard.h"
#elif defined CONNECT_TYPE_FULLSPEC
#include "hal/connection_type/connection_fullspec.h"
#elif defined CONNECT_TYPE_AIBOM
#include "hal/connection_type/connection_aibom.h"
#elif defined CONNECT_TYPE_GAMERNIUM
#include "hal/connection_type/connection_gamernium.h"
#else
#error Invalid connection type or none specified
#endif

using namespace std;
using namespace spdlog;

string current_log_level = "unknown"; // Some versions of spdlog do not support get_log_level()

void ScsiLoop::Banner(const vector<char *> &args) const
{
    cout << piscsi_util::Banner("(SCSI Loopback Test)");
    cout << "Connection type: " << CONNECT_DESC << '\n' << flush;

    if ((args.size() > 1 && strcmp(args[1], "-h") == 0) || (args.size() > 1 && strcmp(args[1], "--help") == 0)) {
        cout << "\nUsage: " << args[0] << " [-L log_level] ...\n\n";
        exit(EXIT_SUCCESS);
    }
}

void ScsiLoop::TerminationHandler(int signum)
{
    exit(signum);
}

bool ScsiLoop::ParseArgument(const vector<char *> &args)
{
    string name;
    string log_level;

    const char *locale = setlocale(LC_MESSAGES, "");
    if (locale == nullptr || !strcmp(locale, "C")) {
        locale = "en";
    }

    opterr = 1;
    int opt;

    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "-L:")) != -1) {
        switch (opt) {
        case 'L':
            log_level = optarg;
            continue;

        default:
            return false;
        }

        if (optopt) {
            return false;
        }
    }

    if (!log_level.empty()) {
        piscsi_log_level::set_log_level(log_level);
    }

    return true;
}

int ScsiLoop::run(const vector<char *> &args)
{
    // added setvbuf to override stdout buffering, so logs are written immediately and not when the process exits.
    setvbuf(stdout, nullptr, _IONBF, 0);

    // Output the Banner
    Banner(args);

    // ParseArgument() requires the bus to have been initialized first, which requires the root user.
    // The -v option should be available for any user, which requires special handling.
    for (auto this_arg : args) {
        if (!strcasecmp(this_arg, "-v")) {
            cout << piscsi_get_version_string() << endl;
            return 0;
        }
    }

    // Create a thread-safe stdout logger to process the log messages
    const auto logger = stdout_color_mt("scsiloop stdout logger");
    set_level(level::info);
    current_log_level = "info";

    vector<string> error_list;

    if (!ParseArgument(args)) {
        return -1;
    }

    // Signal handler to detach all devices on a KILL or TERM signal
    struct sigaction termination_handler;
    termination_handler.sa_handler = TerminationHandler;
    sigemptyset(&termination_handler.sa_mask);
    termination_handler.sa_flags = 0;
    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);

    // This must be executed before the timer test, since this initializes the timer
    ScsiLoop_GPIO gpio_test;

    int result = ScsiLoop_Timer::RunTimerTest(error_list);
    result += gpio_test.RunLoopbackTest(error_list);

    if (result == 0) {
        // Only test the dat inputs/outputs if the loopback test passed.
        result += gpio_test.RunDataInputTest(error_list);
        result += gpio_test.RunDataOutputTest(error_list);
    }

    ScsiLoop_Cout::PrintErrors(error_list);
    gpio_test.Cleanup();

    return 0;
}
