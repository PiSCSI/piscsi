//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#include "os.h"
#include "log.h"
#include "gpiobus.h"
#include "rascsi_version.h"
#include "spdlog/spdlog.h"
#include <sys/time.h>
#include <climits>
#include <sstream>
#include <iostream>
#include <getopt.h>
#include "rascsi.h"
#include <sched.h>
#include "monitor/sm_reports.h"
#include "monitor/data_sample.h"

using namespace std;

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------

// Symbol definition for the VCD file
// These are just arbitrary symbols. They can be anything allowed by the VCD file format,
// as long as they're consistently used.
#define SYMBOL_PIN_DAT '#'
#define SYMBOL_PIN_ATN '+'
#define SYMBOL_PIN_RST '$'
#define SYMBOL_PIN_ACK '%'
#define SYMBOL_PIN_REQ '^'
#define SYMBOL_PIN_MSG '&'
#define SYMBOL_PIN_CD '*'
#define SYMBOL_PIN_IO '('
#define SYMBOL_PIN_BSY ')'
#define SYMBOL_PIN_SEL '-'
#define SYMBOL_PIN_PHASE '='

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
static volatile bool running; // Running flag
GPIOBUS *bus;                 // GPIO Bus

DWORD buff_size = 1000000;
data_capture *data_buffer;
DWORD data_idx = 0;

double ns_per_loop;

bool print_help = false;
bool import_data = false;

// We don't really need to support 256 character file names - this causes
// all kinds of compiler warnings when the log filename can be up to 256
// characters. _MAX_FNAME/2 is just an arbitrary value.
char file_base_name[_MAX_FNAME / 2] = "log";
char vcd_file_name[_MAX_FNAME];
char json_file_name[_MAX_FNAME];
char html_file_name[_MAX_FNAME];
char input_file_name[_MAX_FNAME];

//---------------------------------------------------------------------------
//
//	Signal Processing
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
    // Stop instruction
    running = false;
}

void parse_arguments(int argc, char *argv[])
{
    int opt;

    while ((opt = getopt(argc, argv, "-Hhb:i:")) != -1)
    {
        switch (opt)
        {
        // The three options below are kind of a compound option with two letters
        case 'h':
        case 'H':
            print_help = true;
            break;
        case 'b':
            buff_size = atoi(optarg);
            break;
        case 'i':
            strncpy(input_file_name, optarg, sizeof(input_file_name)-1);
            import_data = true;
            break;
        case 1:
            strncpy(file_base_name, optarg, sizeof(file_base_name) - 5);
            break;
        default:
            cout << "default: " << optarg << endl;
            break;
        }
    }

    /* Process any remaining command line arguments (not options). */
    if (optind < argc)
    {
        while (optind < argc)
            strncpy(file_base_name, argv[optind++], sizeof(file_base_name)-1);
    }

    strcpy(vcd_file_name, file_base_name);
    strcat(vcd_file_name, ".vcd");
    strcpy(json_file_name, file_base_name);
    strcat(json_file_name, ".json");
    strcpy(html_file_name, file_base_name);
    strcat(html_file_name, ".html");
}
//---------------------------------------------------------------------------
//
//	Copyright text
//
//---------------------------------------------------------------------------
void print_copyright_text(int argc, char *argv[])
{
    LOGINFO("SCSI Monitor Capture Tool - part of RaSCSI(*^..^*) ");
    LOGINFO("version %s (%s, %s)",
            rascsi_get_version_string(),
            __DATE__,
            __TIME__);
    LOGINFO("Powered by XM6 TypeG Technology ");
    LOGINFO("Copyright (C) 2016-2020 GIMONS");
    LOGINFO("Copyright (C) 2020-2021 Contributors to the RaSCSI project");
    LOGINFO(" ");
}

//---------------------------------------------------------------------------
//
//	Help text
//
//---------------------------------------------------------------------------
void print_help_text(int argc, char *argv[])
{
    LOGINFO("%s -i [input file json] -b [buffer size] [output file]", argv[0]);
    LOGINFO("       -i [input file json] - scsimon will parse the json file instead of capturing new data");
    LOGINFO("                              If -i option is not specified, scsimon will read the gpio pins");
    LOGINFO("       -b [buffer size]     - Override the default buffer size of %d.", buff_size);
    LOGINFO("       [output file]        - Base name of the output files. The file extension (ex: .json)");
    LOGINFO("                              will be appended to this file name");
}

//---------------------------------------------------------------------------
//
//	Banner Output
//
//---------------------------------------------------------------------------
void Banner(int argc, char *argv[])
{
    if (import_data)
    {
        LOGINFO("Reading input file: %s", input_file_name);
    }
    else
    {
        LOGINFO("Reading live data from the GPIO pins");
        LOGINFO("    Connection type : %s", CONNECT_DESC);
    }
    LOGINFO("    Data buffer size: %u", buff_size);
    LOGINFO(" ");
    LOGINFO("Generating output files:");
    LOGINFO("   %s - Value Change Dump file that can be opened with GTKWave", vcd_file_name);
    LOGINFO("   %s - JSON file with raw data", json_file_name);
    LOGINFO("   %s - HTML file with summary of commands", html_file_name);
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
bool Init()
{
    // Interrupt handler settings
    if (signal(SIGINT, KillHandler) == SIG_ERR)
    {
        return FALSE;
    }
    if (signal(SIGHUP, KillHandler) == SIG_ERR)
    {
        return FALSE;
    }
    if (signal(SIGTERM, KillHandler) == SIG_ERR)
    {
        return FALSE;
    }

    // GPIO Initialization
    bus = new GPIOBUS();
    if (!bus->Init())
    {
        LOGERROR("Unable to intiailize the GPIO bus. Exiting....");
        return false;
    }

    // Bus Reset
    bus->Reset();

    // Other
    running = false;

    return true;
}

void Cleanup()
{
    if (!import_data)
    {
        LOGINFO("Stopping data collection....");
    }
    LOGINFO(" ");
    LOGINFO("Generating %s...", vcd_file_name);
    scsimon_generate_value_change_dump(vcd_file_name, data_buffer, data_idx);
    LOGINFO("Generating %s...", json_file_name);
    scsimon_generate_json(json_file_name, data_buffer, data_idx);
    LOGINFO("Generating %s...", html_file_name);
    scsimon_generate_html(html_file_name, data_buffer, data_idx);

    if (bus)
    {
        // Cleanup the Bus
        bus->Cleanup();
        delete bus;
    }
}

void Reset()
{
    // Reset the bus
    bus->Reset();
}

//---------------------------------------------------------------------------
//
//	Pin the thread to a specific CPU (Only applies to Linux)
//
//---------------------------------------------------------------------------
#ifdef __linux__
void FixCpu(int cpu)
{
    // Get the number of CPUs
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
    int cpus = CPU_COUNT(&cpuset);

    // Set the thread affinity
    if (cpu < cpus)
    {
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    }
}
#endif

#ifdef DEBUG
static DWORD high_bits = 0x0;
static DWORD low_bits = 0xFFFFFFFF;
#endif

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{

#ifdef DEBUG
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    spdlog::set_pattern("%^[%l]%$ %v");

    print_copyright_text(argc, argv);
    parse_arguments(argc, argv);

#ifdef DEBUG
    DWORD prev_high = high_bits;
    DWORD prev_low = low_bits;
#endif
    ostringstream s;
    DWORD prev_sample = 0xFFFFFFFF;
    DWORD this_sample = 0;
    timeval start_time;
    timeval stop_time;
    uint64_t loop_count = 0;
    timeval time_diff;
    uint64_t elapsed_us;

    if (print_help)
    {
        print_help_text(argc, argv);
        exit(0);
    }

    // Output the Banner
    Banner(argc, argv);

    data_buffer = (data_capture *)malloc(sizeof(data_capture_t) * buff_size);
    bzero(data_buffer, sizeof(data_capture_t) * buff_size);

    if (import_data)
    {
        data_idx = scsimon_read_json(input_file_name, data_buffer, buff_size);
        if (data_idx > 0)
        {
            LOGDEBUG("Read %d samples from %s", data_idx, input_file_name);
            Cleanup();
        }
        exit(0);
    }

    LOGINFO(" ");
    LOGINFO("Now collecting data.... Press CTRL-C to stop.")
    LOGINFO(" ");

    // Initialize
    int ret = 0;
    if (!Init())
    {
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
    bus->SetACT(FALSE);

    (void)gettimeofday(&start_time, NULL);

    LOGDEBUG("ALL_SCSI_PINS %08X\n", ALL_SCSI_PINS);

    // Main Loop
    while (running)
    {
        // Work initialization
        this_sample = (bus->Aquire() & ALL_SCSI_PINS);
        loop_count++;
        if (loop_count > LLONG_MAX - 1)
        {
            LOGINFO("Maximum amount of time has elapsed. SCSIMON is terminating.");
            running = false;
        }
        if (data_idx >= (buff_size - 2))
        {
            LOGINFO("Internal data buffer is full. SCSIMON is terminating.");
            running = false;
        }

        if (this_sample != prev_sample)
        {

#ifdef DEBUG
            // This is intended to be a debug check to see if every pin is set
            // high and low at some point.
            high_bits |= this_sample;
            low_bits &= this_sample;
            if ((high_bits != prev_high) || (low_bits != prev_low))
            {
                LOGDEBUG("   %08X    %08X\n", high_bits, low_bits);
            }
            prev_high = high_bits;
            prev_low = low_bits;
            if ((data_idx % 1000) == 0)
            {
                s.str("");
                s << "Collected " << data_idx << " samples...";
                LOGDEBUG("%s", s.str().c_str());
            }
#endif
            data_buffer[data_idx].data = this_sample;
            data_buffer[data_idx].timestamp = loop_count;
            data_idx++;
            prev_sample = this_sample;
        }

        continue;
    }

    // Collect one last sample, otherwise it looks like the end of the data was cut off
    if (data_idx < buff_size)
    {
        data_buffer[data_idx].data = this_sample;
        data_buffer[data_idx].timestamp = loop_count;
        data_idx++;
    }

    (void)gettimeofday(&stop_time, NULL);

    timersub(&stop_time, &start_time, &time_diff);

    elapsed_us = ((time_diff.tv_sec * 1000000) + time_diff.tv_usec);
    s.str("");
    s << "Elapsed time: " << elapsed_us << " microseconds (" << elapsed_us / 1000000 << " seconds)";
    LOGINFO("%s", s.str().c_str());
    s.str("");
    s << "Collected " << data_idx << " changes";
    LOGINFO("%s", s.str().c_str());

    // Note: ns_per_loop is a global variable that is used by Cleanup() to printout the timestamps.
    ns_per_loop = (elapsed_us * 1000) / (double)loop_count;
    s.str("");
    s << "Read the SCSI bus " << loop_count << " times with an average of " << ns_per_loop << " ns for each read";
    LOGINFO("%s", s.str().c_str());

    Cleanup();

init_exit:
    exit(ret);
}
