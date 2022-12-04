//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2021-2022 akuker
//
//---------------------------------------------------------------------------

#include "monitor/sm_core.h"
#include "hal/data_sample.h"
#include "hal/gpiobus.h"
#include "hal/gpiobus_factory.h"
#include "monitor/sm_reports.h"
#include "shared/log.h"
#include "shared/piscsi_version.h"
#include "shared/pisutil.h"
#include <climits>
#include <csignal>
#include <getopt.h>
#include <iostream>
#include <sched.h>
#include <sys/time.h>

using namespace std;
using namespace ras_util;

void ScsiMon::KillHandler(int)
{
    running = false;
}

void ScsiMon::ParseArguments(const vector<char *> &args)
{
    int opt;

    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "-Hhb:i:")) != -1) {
        switch (opt) {
        // The three options below are kind of a compound option with two letters
        case 'h':
        case 'H':
            print_help = true;
            break;
        case 'b':
            buff_size = atoi(optarg);
            break;
        case 'i':
            input_file_name = optarg;
            import_data     = true;
            break;
        case 1:
            file_base_name = optarg;
            break;
        default:
            cout << "default: " << optarg << endl;
            break;
        }
    }

    /* Process any remaining command line arguments (not options). */
    if (optind < static_cast<int>(args.size())) {
        while (optind < static_cast<int>(args.size())) {
            file_base_name = args[optind];
            optind++;
        }
    }

    vcd_file_name = file_base_name;
    vcd_file_name += ".vcd";
    json_file_name = file_base_name;
    json_file_name += ".json";
    html_file_name = file_base_name;
    html_file_name += ".html";
}

void ScsiMon::PrintHelpText(const vector<char *> &args) const
{
    LOGINFO("%s -i [input file json] -b [buffer size] [output file]", args[0])
    LOGINFO("       -i [input file json] - scsimon will parse the json file instead of capturing new data")
    LOGINFO("                              If -i option is not specified, scsimon will read the gpio pins")
    LOGINFO("       -b [buffer size]     - Override the default buffer size of %d.", buff_size)
    LOGINFO("       [output file]        - Base name of the output files. The file extension (ex: .json)")
    LOGINFO("                              will be appended to this file name")
}

void ScsiMon::Banner() const
{
    if (import_data) {
        LOGINFO("Reading input file: %s", input_file_name.c_str())
    } else {
        LOGINFO("Reading live data from the GPIO pins")
        LOGINFO("    Connection type : %s", CONNECT_DESC.c_str())
    }
    LOGINFO("    Data buffer size: %u", buff_size)
    LOGINFO(" ")
    LOGINFO("Generating output files:")
    LOGINFO("   %s - Value Change Dump file that can be opened with GTKWave", vcd_file_name.c_str())
    LOGINFO("   %s - JSON file with raw data", json_file_name.c_str())
    LOGINFO("   %s - HTML file with summary of commands", html_file_name.c_str())
}

bool ScsiMon::Init()
{
    // Interrupt handler settings
    if (signal(SIGINT, KillHandler) == SIG_ERR) {
        return false;
    }
    if (signal(SIGHUP, KillHandler) == SIG_ERR) {
        return false;
    }
    if (signal(SIGTERM, KillHandler) == SIG_ERR) {
        return false;
    }

    bus = GPIOBUS_Factory::Create(BUS::mode_e::TARGET);
    if (bus == nullptr) {
        LOGERROR("Unable to intiailize the GPIO bus. Exiting....")
        return false;
    }

    running = false;

    return true;
}

void ScsiMon::Cleanup() const
{
    if (!import_data) {
        LOGINFO("Stopping data collection....")
    }
    LOGINFO(" ")
    LOGINFO("Generating %s...", vcd_file_name.c_str())
    scsimon_generate_value_change_dump(vcd_file_name, data_buffer);
    LOGINFO("Generating %s...", json_file_name.c_str())
    scsimon_generate_json(json_file_name, data_buffer);
    LOGINFO("Generating %s...", html_file_name.c_str())
    scsimon_generate_html(html_file_name, data_buffer);

    bus->Cleanup();
}

void ScsiMon::Reset() const
{
    bus->Reset();
}

int ScsiMon::run(const vector<char *> &args)
{
#ifdef DEBUG
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    spdlog::set_pattern("%^[%l]%$ %v");

    ras_util::Banner("SCSI Monitor Capture Tool");

    ParseArguments(args);

    shared_ptr<DataSample> prev_sample = nullptr;
    shared_ptr<DataSample> this_sample = nullptr;
    timeval start_time;
    timeval stop_time;
    uint64_t loop_count = 0;
    timeval time_diff;
    uint64_t elapsed_us;

    if (print_help) {
        PrintHelpText(args);
        exit(0);
    }

    Banner();

    data_buffer.reserve(buff_size);

    if (import_data) {
        data_idx = scsimon_read_json(input_file_name, data_buffer);
        if (data_idx > 0) {
            LOGDEBUG("Read %d samples from %s", data_idx, input_file_name.c_str())
            Cleanup();
        }
        exit(0);
    }

    LOGINFO(" ")
    LOGINFO("Now collecting data.... Press CTRL-C to stop.")
    LOGINFO(" ")

    // Initialize
    int ret = 0;
    if (!Init()) {
        ret = EPERM;
        goto init_exit;
    }

    // Reset
    Reset();

#ifdef __linux__
    // Set the affinity to a specific processor core
    FixCpu(3);

    // Scheduling policy setting (highest priority)
    struct sched_param schparam;
    schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
    sched_setscheduler(0, SCHED_FIFO, &schparam);
#endif

    // Start execution
    running = true;
    bus->SetACK(false);

    (void)gettimeofday(&start_time, nullptr);

    // Main Loop
    while (running) {
        loop_count++;
        if (loop_count > LLONG_MAX - 1) {
            LOGINFO("Maximum amount of time has elapsed. SCSIMON is terminating.")
            running = false;
        }

        if (data_idx >= (buff_size - 2)) {
            LOGINFO("Internal data buffer is full. SCSIMON is terminating.")
            running = false;
        }

        this_sample = bus->GetSample(loop_count);
        if ((prev_sample == nullptr) || (this_sample->GetRawCapture() != prev_sample->GetRawCapture())) {
            data_buffer.push_back(this_sample);
            data_idx++;
            prev_sample = this_sample;
        }

        continue;
    }

    // Collect one last sample, otherwise it looks like the end of the data was cut off
    if (data_idx < buff_size) {
        data_buffer.push_back(bus->GetSample(loop_count));
        data_idx++;
    }

    (void)gettimeofday(&stop_time, nullptr);

    timersub(&stop_time, &start_time, &time_diff);

    elapsed_us = ((time_diff.tv_sec * 1000000) + time_diff.tv_usec);
    LOGINFO("%s", ("Elapsed time: " + to_string(elapsed_us) + " microseconds (" + to_string(elapsed_us / 1000000) +
                   " seconds")
                      .c_str())
    LOGINFO("%s", ("Collected " + to_string(data_idx) + " changes").c_str())

    ns_per_loop = (double)(elapsed_us * 1000) / (double)loop_count;
    LOGINFO("%s", ("Read the SCSI bus " + to_string(loop_count) + " times with an average of " +
                   to_string(ns_per_loop) + " ns for each read")
                      .c_str())

    Cleanup();

init_exit:
    exit(ret);
}
