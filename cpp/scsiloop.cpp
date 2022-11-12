//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded for Raspberry Pi
//  Loopback tester utility
//
//	Copyright (C) 2022 akuker
//
//	[ Loopback tester utility ]
//
//  For more information, see:
//     https://github.com/akuker/RASCSI/wiki/Troubleshooting#Loopback_Testing
//
//---------------------------------------------------------------------------

#include "hal/gpiobus.h"
#include "hal/gpiobus_factory.h"
#include "hal/sbc_version.h"
#include "hal/systimer.h"
#include "shared/log.h"
#include "shared/rascsi_version.h"
#include "shared/rasutil.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include <sched.h>
#include <stdio.h>
#include <signal.h>

#if defined CONNECT_TYPE_STANDARD
#include "hal/gpiobus_standard.h"
#elif defined CONNECT_TYPE_FULLSPEC
#include "hal/gpiobus_fullspec.h"
#elif defined CONNECT_TYPE_AIBOM
#include "hal/gpiobus_aibom.h"
#elif defined CONNECT_TYPE_GAMERNIUM
#include "hal/gpiobus_gamernium.h"
#else
#error Invalid connection type or none specified
#endif

using namespace std;
using namespace spdlog;

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------
#define RESET "\033[0m"
#define BLACK "\033[30m"   /* Black */
#define RED "\033[31m"     /* Red */
#define GREEN "\033[32m"   /* Green */
#define YELLOW "\033[33m"  /* Yellow */
#define BLUE "\033[34m"    /* Blue */
#define MAGENTA "\033[35m" /* Magenta */
#define CYAN "\033[36m"    /* Cyan */
#define WHITE "\033[37m"   /* White */

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
shared_ptr<GPIOBUS> bus;
string current_log_level = "debug"; // Some versions of spdlog do not support get_log_level()

// string connection_type = "hard-coded fullspec";

            int local_pin_dtd = -1;
        int local_pin_tad = -1;
        int local_pin_ind = -1;

void Banner(int argc, char *argv[])
{
    cout << ras_util::Banner("Reloaded");
    cout << "Connect type: " << CONNECT_DESC << '\n' << flush;
    cout << "Board type: " << SBC_Version::GetString() << endl << flush;

    if ((argc > 1 && strcmp(argv[1], "-h") == 0) || (argc > 1 && strcmp(argv[1], "--help") == 0)) {
        cout << "\nUsage: " << argv[0] << " [-idn[:m] FILE] ...\n\n";
        cout << " n is SCSI device ID (0-7).\n";
        cout << " m is the optional logical unit (LUN) (0-31).\n";
        cout << " FILE is a disk image file, \"daynaport\", \"bridge\", \"printer\" or \"services\".\n\n";
        cout << " Image type is detected based on file extension if no explicit type is specified.\n";
        cout << "  hd1 : SCSI-1 HD image (Non-removable generic SCSI-1 HD image)\n";
        cout << "  hds : SCSI HD image (Non-removable generic SCSI HD image)\n";
        cout << "  hdr : SCSI HD image (Removable generic HD image)\n";
        cout << "  hda : SCSI HD image (Apple compatible image)\n";
        cout << "  hdn : SCSI HD image (NEC compatible image)\n";
        cout << "  hdi : SCSI HD image (Anex86 HD image)\n";
        cout << "  nhd : SCSI HD image (T98Next HD image)\n";
        cout << "  mos : SCSI MO image (MO image)\n";
        cout << "  iso : SCSI CD image (ISO 9660 image)\n" << flush;

        exit(EXIT_SUCCESS);
    }
}

bool InitBus()
{
#ifdef USE_SEL_EVENT_ENABLE
    SBC_Version::Init();
#endif

	bus = GPIOBUS_Factory::Create(BUS::mode_e::TARGET);
	if (bus == nullptr) {
		return false;
	}

    return true;
}

void Cleanup()
{
    bus->Cleanup();
}

void Reset()
{
    bus->Reset();
}

void test_timer();
void test_gpio();
void run_loopback_test();
void set_dtd_out();
void set_dtd_in();
void set_ind_out();
void set_ind_in();
void set_tad_out();
void set_tad_in();
void tony_test();
void print_all();

//---------------------------------------------------------------------------
//
//	Pin the thread to a specific CPU
//
//---------------------------------------------------------------------------
void FixCpu(int cpu)
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    // Get the number of CPUs
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
    int cpus = CPU_COUNT(&cpuset);

    // Set the thread affinity
    if (cpu < cpus) {
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    }
}

bool SetLogLevel(const string &log_level)
{
    if (log_level == "trace") {
        set_level(level::trace);
    } else if (log_level == "debug") {
        set_level(level::debug);
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

void TerminationHandler(int signum)
{
    Cleanup();
    exit(signum);
}

bool ParseArgument(int argc, char *argv[])
{
    string name;
    string log_level;

    const char *locale = setlocale(LC_MESSAGES, "");
    if (locale == nullptr || !strcmp(locale, "C")) {
        locale = "en";
    }

    opterr = 1;
    int opt;
    while ((opt = getopt(argc, argv, "-Iib:d:n:p:r:t:z:D:F:L:P:R:")) != -1) {
        switch (opt) {
        case 'z':
            locale = optarg;
            continue;

        case 'L':
            log_level = optarg;
            continue;

        case 'n':
            name = optarg;
            continue;

        case 1:
            // Encountered filename
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

int main(int argc, char *argv[])
{
    // added setvbuf to override stdout buffering, so logs are written immediately and not when the process exits.
    setvbuf(stdout, nullptr, _IONBF, 0);

    // Output the Banner
    Banner(argc, argv);

    // ParseArgument() requires the bus to have been initialized first, which requires the root user.
    // The -v option should be available for any user, which requires special handling.
    for (int i = 1; i < argc; i++) {
        if (!strcasecmp(argv[i], "-v")) {
            cout << rascsi_get_version_string() << endl;
            return 0;
        }
    }

    // executor->SetLogLevel(current_log_level);

    // Create a thread-safe stdout logger to process the log messages
    const auto logger = stdout_color_mt("rascsi stdout logger");
    set_level(level::info);

    if (!InitBus()) {
        return EPERM;
    }

    if (!ParseArgument(argc, argv)) {
        Cleanup();
        return -1;
    }

    // Signal handler to detach all devices on a KILL or TERM signal
    struct sigaction termination_handler;
    termination_handler.sa_handler = TerminationHandler;
    sigemptyset(&termination_handler.sa_mask);
    termination_handler.sa_flags = 0;
    sigaction(SIGINT, &termination_handler, nullptr);
    sigaction(SIGTERM, &termination_handler, nullptr);

    // Reset
    Reset();

    // Set the affinity to a specific processor core
    FixCpu(3);

#ifdef USE_SEL_EVENT_ENABLE
    sched_param schparam;
    // Scheduling policy setting (highest priority)
    schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
    sched_setscheduler(0, SCHED_FIFO, &schparam);
#else
    cout << "Note: No RaSCSI hardware support, only client interface calls are supported" << endl;
#endif

    run_loopback_test();
    return 0;
}

void test_timer()
{
    uint32_t before = SysTimer::GetTimerLow();
    sleep(1);
    uint32_t after = SysTimer::GetTimerLow();

    LOGINFO("first sample: %d %08X", (before - after), (after - before));

    uint64_t sum = 0;
    int count    = 0;
    for (int i = 0; i < 100; i++) {
        before = SysTimer::GetTimerLow();
        usleep(1000);
        after = SysTimer::GetTimerLow();
        sum += (after - before);
        count++;
    }

    LOGINFO("usleep() Average %d", (uint32_t)(sum / count));

    sum   = 0;
    count = 0;
    for (int i = 0; i < 100; i++) {
        before = SysTimer::GetTimerLow();
        SysTimer::SleepUsec(1000);
        after = SysTimer::GetTimerLow();
        sum += (after - before);
        count++;
    }
    LOGINFO("usleep() Average %d", (uint32_t)(sum / count));

    before = SysTimer::GetTimerLow();
    SysTimer::SleepNsec(1000000);
    after = SysTimer::GetTimerLow();
    LOGINFO("SysTimer::SleepNSec: %d (expected ~1000)", (uint32_t)(after - before));
}

struct loopback_connections_struct {
    int this_pin;
    int connected_pin;
    int dir_ctrl_pin;
};
typedef loopback_connections_struct loopback_connection;

std::map<int, string> pin_name_lookup;
std::vector<loopback_connection> loopback_conn_table;

// This needs to run AFTER GPIOBUS has been initialized. Otherwise, we don't know what type of board
// we're using
void init_loopback()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    // shared_ptr<board_type::Rascsi_Board_Type> board_table = bus->board;

    if (SBC_Version::IsRaspberryPi()) {
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT0, .connected_pin = PIN_ACK, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT1, .connected_pin = PIN_SEL, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT2, .connected_pin = PIN_ATN, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT3, .connected_pin = PIN_RST, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT4, .connected_pin = PIN_CD, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT5, .connected_pin = PIN_IO, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT6, .connected_pin = PIN_MSG, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DT7, .connected_pin = PIN_REQ, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_DP, .connected_pin = PIN_BSY, .dir_ctrl_pin = PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_ATN, .connected_pin = PIN_DT2, .dir_ctrl_pin = PIN_IND});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_RST, .connected_pin = PIN_DT3, .dir_ctrl_pin = PIN_IND});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_ACK, .connected_pin = PIN_DT0, .dir_ctrl_pin = PIN_IND});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_REQ, .connected_pin = PIN_DT7, .dir_ctrl_pin = PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_MSG, .connected_pin = PIN_DT6, .dir_ctrl_pin = PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_CD, .connected_pin = PIN_DT4, .dir_ctrl_pin = PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_IO, .connected_pin = PIN_DT5, .dir_ctrl_pin = PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_BSY, .connected_pin = PIN_DP, .dir_ctrl_pin = PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = PIN_SEL, .connected_pin = PIN_DT1, .dir_ctrl_pin = PIN_IND});
        pin_name_lookup[PIN_DT0] = " d0";
        pin_name_lookup[PIN_DT1] = " d1";
        pin_name_lookup[PIN_DT2] = " d2";
        pin_name_lookup[PIN_DT3] = " d3";
        pin_name_lookup[PIN_DT4] = " d4";
        pin_name_lookup[PIN_DT5] = " d5";
        pin_name_lookup[PIN_DT6] = " d6";
        pin_name_lookup[PIN_DT7] = " d7";
        pin_name_lookup[PIN_DP]  = " dp";
        pin_name_lookup[PIN_ATN] = "atn";
        pin_name_lookup[PIN_RST] = "rst";
        pin_name_lookup[PIN_ACK] = "ack";
        pin_name_lookup[PIN_REQ] = "req";
        pin_name_lookup[PIN_MSG] = "msg";
        pin_name_lookup[PIN_CD]  = " cd";
        pin_name_lookup[PIN_IO]  = " io";
        pin_name_lookup[PIN_BSY] = "bsy";
        pin_name_lookup[PIN_SEL] = "sel";
        pin_name_lookup[PIN_IND] = "ind";
        pin_name_lookup[PIN_TAD] = "tad";
        pin_name_lookup[PIN_DTD] = "dtd";

        local_pin_dtd = PIN_DTD;
        local_pin_tad = PIN_TAD;
        local_pin_ind = PIN_IND;
    } else if (SBC_Version::GetSbcVersion() == SBC_Version::sbc_version_type::sbc_bananapi_m2_plus) {
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_DT0, .connected_pin = BPI_PIN_ACK, .dir_ctrl_pin = BPI_PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_DT1, .connected_pin = BPI_PIN_SEL, .dir_ctrl_pin = BPI_PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_DT2, .connected_pin = BPI_PIN_ATN, .dir_ctrl_pin = BPI_PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_DT3, .connected_pin = BPI_PIN_RST, .dir_ctrl_pin = BPI_PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_DT4, .connected_pin = BPI_PIN_CD, .dir_ctrl_pin = BPI_PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_DT5, .connected_pin = BPI_PIN_IO, .dir_ctrl_pin = BPI_PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_DT6, .connected_pin = BPI_PIN_MSG, .dir_ctrl_pin = BPI_PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_DT7, .connected_pin = BPI_PIN_REQ, .dir_ctrl_pin = BPI_PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_DP, .connected_pin = BPI_PIN_BSY, .dir_ctrl_pin = BPI_PIN_DTD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_ATN, .connected_pin = BPI_PIN_DT2, .dir_ctrl_pin = BPI_PIN_IND});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_RST, .connected_pin = BPI_PIN_DT3, .dir_ctrl_pin = BPI_PIN_IND});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_ACK, .connected_pin = BPI_PIN_DT0, .dir_ctrl_pin = BPI_PIN_IND});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_REQ, .connected_pin = BPI_PIN_DT7, .dir_ctrl_pin = BPI_PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_MSG, .connected_pin = BPI_PIN_DT6, .dir_ctrl_pin = BPI_PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_CD, .connected_pin = BPI_PIN_DT4, .dir_ctrl_pin = BPI_PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_IO, .connected_pin = BPI_PIN_DT5, .dir_ctrl_pin = BPI_PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_BSY, .connected_pin = BPI_PIN_DP, .dir_ctrl_pin = BPI_PIN_TAD});
        loopback_conn_table.push_back(
            loopback_connection{.this_pin = BPI_PIN_SEL, .connected_pin = BPI_PIN_DT1, .dir_ctrl_pin = BPI_PIN_IND});

        pin_name_lookup[BPI_PIN_DT0] = " d0";
        pin_name_lookup[BPI_PIN_DT1] = " d1";
        pin_name_lookup[BPI_PIN_DT2] = " d2";
        pin_name_lookup[BPI_PIN_DT3] = " d3";
        pin_name_lookup[BPI_PIN_DT4] = " d4";
        pin_name_lookup[BPI_PIN_DT5] = " d5";
        pin_name_lookup[BPI_PIN_DT6] = " d6";
        pin_name_lookup[BPI_PIN_DT7] = " d7";
        pin_name_lookup[BPI_PIN_DP]  = " dp";
        pin_name_lookup[BPI_PIN_ATN] = "atn";
        pin_name_lookup[BPI_PIN_RST] = "rst";
        pin_name_lookup[BPI_PIN_ACK] = "ack";
        pin_name_lookup[BPI_PIN_REQ] = "req";
        pin_name_lookup[BPI_PIN_MSG] = "msg";
        pin_name_lookup[BPI_PIN_CD]  = " cd";
        pin_name_lookup[BPI_PIN_IO]  = " io";
        pin_name_lookup[BPI_PIN_BSY] = "bsy";
        pin_name_lookup[BPI_PIN_SEL] = "sel";
        pin_name_lookup[BPI_PIN_IND] = "ind";
        pin_name_lookup[BPI_PIN_TAD] = "tad";
        pin_name_lookup[BPI_PIN_DTD] = "dtd";
            local_pin_dtd = BPI_PIN_DTD;
        local_pin_tad = BPI_PIN_TAD;
        local_pin_ind = BPI_PIN_IND;

    } else {
        LOGERROR("Unsupported board version: %s", SBC_Version::GetString()->c_str());
    }
}

// Debug function that just dumps the status of all of the scsi signals to the console
void print_all()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    for (auto cur_gpio : loopback_conn_table) {
        LOGDEBUG("%s %2d = %s %2d", pin_name_lookup.at(cur_gpio.this_pin).c_str(), (int)cur_gpio.this_pin,
                 pin_name_lookup.at(cur_gpio.connected_pin).c_str(), (int)cur_gpio.connected_pin)
    }
}

// Set transceivers IC1 and IC2 to OUTPUT
void set_dtd_out()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_dtd, DTD_OUT);
}

// Set transceivers IC1 and IC2 to INPUT
void set_dtd_in()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_dtd, DTD_IN);
}
// Set transceiver IC4 to OUTPUT
void set_ind_out()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_ind, IND_OUT);
}
// Set transceiver IC4 to INPUT
void set_ind_in()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_ind, IND_IN);
}
// Set transceiver IC3 to OUTPUT
void set_tad_out()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_tad, TAD_OUT);
}
// Set transceiver IC3 to INPUT
void set_tad_in()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    bus->SetControl(local_pin_tad, TAD_IN);
}

// Set the specified transciever to an OUTPUT. All of the other transceivers
// will be set to inputs. If a non-existent direction gpio is specified, this
// will set all of the transceivers to inputs.
void set_output_channel(int out_gpio)
{
    LOGTRACE("%s tad: %d dtd: %d ind: %d", CONNECT_DESC.c_str(), (int)local_pin_tad,
             (int)local_pin_dtd, (int)local_pin_ind);
    if (out_gpio == local_pin_tad)
        set_tad_out();
    else
        set_tad_in();
    if (out_gpio == local_pin_dtd)
        set_dtd_out();
    else
        set_dtd_in();
    if (out_gpio ==  local_pin_ind)
        set_ind_out();
    else
        set_ind_in();
}

// Initialize the GPIO library, set all of the gpios associated with SCSI signals to outputs and set
// all of the direction control gpios to outputs
void loopback_setup()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    for (loopback_connection cur_gpio : loopback_conn_table) {
        if (cur_gpio.this_pin == -1) {
            continue;
        }
        bus->PinConfig(cur_gpio.this_pin, GPIO_OUTPUT);
        bus->PullConfig(cur_gpio.this_pin, GPIO_PULLNONE);
    }

    // Setup direction control
    if (local_pin_ind != -1) {
        bus->PinConfig(local_pin_ind, GPIO_OUTPUT);
    }
    if (local_pin_tad != -1) {
        bus->PinConfig(local_pin_tad, GPIO_OUTPUT);
    }
    if (local_pin_dtd != -1) {
        bus->PinConfig(local_pin_dtd, GPIO_OUTPUT);
    }
}

// Main test procedure.This will execute for each of the SCSI pins to make sure its connected
// properly.
int test_gpio_pin(loopback_connection &gpio_rec)
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    int err_count  = 0;
    int sleep_time = 500000; // 5ms

    LOGTRACE("dir ctrl pin: %d", (int)gpio_rec.dir_ctrl_pin);
    set_output_channel(gpio_rec.dir_ctrl_pin);
    usleep(sleep_time);

    // Set all GPIOs high (initialize to a known state)
    for (auto cur_gpio : loopback_conn_table) {
        // bus->SetSignal(cur_gpio.this_pin, OFF);
        bus->SetMode(cur_gpio.this_pin, GPIO_INPUT);
    }

    usleep(sleep_time);
    bus->Acquire();

    // ############################################
    // set the test gpio low
    bus->SetMode(gpio_rec.this_pin, GPIO_OUTPUT);
    bus->SetSignal(gpio_rec.this_pin, ON);

    usleep(sleep_time);

    bus->Acquire();
    // loop through all of the gpios
    for (auto cur_gpio : loopback_conn_table) {
        printf(".");
        // all of the gpios should be high except for the test gpio and the connected gpio
        LOGTRACE("calling bus->GetSignal(%d)", (int)cur_gpio.this_pin);
        auto cur_val = bus->GetSignal(cur_gpio.this_pin);
        LOGDEBUG("%d [%s] is %d", (int)cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin).c_str(), (int)cur_val);

        if (cur_gpio.this_pin == gpio_rec.this_pin) {
            if (cur_val != ON) {
                LOGERROR("Test commanded GPIO %d [%s] to be low, but it did not respond", (int)cur_gpio.this_pin,
                         pin_name_lookup.at(cur_gpio.this_pin).c_str())
                err_count++;
            }
        } else if (cur_gpio.this_pin == gpio_rec.connected_pin) {
            if (cur_val != ON) {
                LOGERROR("GPIO %d [%s] should be driven low, but %d [%s] did not affect it", (int)cur_gpio.this_pin,
                         pin_name_lookup.at(cur_gpio.this_pin).c_str(), (int)cur_gpio.connected_pin,
                         pin_name_lookup.at(cur_gpio.connected_pin).c_str());

                err_count++;
            }
        } else {
            if (cur_val != OFF) {
                LOGERROR("GPIO %d [%s] was incorrectly pulled low, when it shouldn't be", (int)cur_gpio.this_pin,
                         pin_name_lookup.at(cur_gpio.this_pin).c_str())
                err_count++;
            }
        }
    }

    // ############################################
    // set the transceivers to input
    set_output_channel(-1);

    usleep(sleep_time);

    // # loop through all of the gpios
    for (auto cur_gpio : loopback_conn_table) {
        printf(".");
        // all of the gpios should be high except for the test gpio
        LOGTRACE("calling bus->GetSignal(%d)", (int)cur_gpio.this_pin);
        auto cur_val = bus->GetSignal(cur_gpio.this_pin);
        LOGDEBUG("%d [%s] is %d", (int)cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin).c_str(), (int)cur_val);

        if (cur_gpio.this_pin == gpio_rec.this_pin) {
            if (cur_val != ON) {
                LOGERROR("Test commanded GPIO %d [%s] to be low, but it did not respond", (int)cur_gpio.this_pin,
                         pin_name_lookup.at(cur_gpio.this_pin).c_str())
                err_count++;
            }
        } else {
            if (cur_val != OFF) {
                LOGERROR("GPIO %d [%s] was incorrectly pulled low, when it shouldn't be", (int)cur_gpio.this_pin,
                         pin_name_lookup.at(cur_gpio.this_pin).c_str())
                err_count++;
                LOGINFO("TRY IT AGAIN")
                bus->Acquire();
                auto new_val = bus->GetSignal(cur_gpio.this_pin);
                LOGINFO("Connected pin is %d (%s)", new_val, (new_val) ? "OFF" : "ON")
            }
        }
    }
    // exit(1);

    // Set the transceiver back to output
    set_output_channel(gpio_rec.dir_ctrl_pin);
    usleep(sleep_time);

    // #############################################
    // set the test gpio high
    bus->SetMode(gpio_rec.this_pin, GPIO_OUTPUT);
    bus->SetSignal(gpio_rec.this_pin, OFF);

    usleep(sleep_time);

    bus->Acquire();
    // loop through all of the gpios
    for (auto cur_gpio : loopback_conn_table) {
        printf(".");

        auto cur_val = bus->GetSignal(cur_gpio.this_pin);
        LOGTRACE("%d [%s] is %d", (int)cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin).c_str(), (int)cur_val);

        if (cur_gpio.this_pin == gpio_rec.this_pin) {
            if (cur_val != OFF) {
                LOGERROR("Test commanded GPIO %d [%s] to be high, but it did not respond", (int)cur_gpio.this_pin,
                         pin_name_lookup.at(cur_gpio.this_pin).c_str())
                err_count++;
            }
        } else {
            if (cur_val != OFF) {
                LOGERROR("GPIO %d [%s] was incorrectly pulled low, when it shouldn't be", (int)cur_gpio.this_pin,
                         pin_name_lookup.at(cur_gpio.this_pin).c_str())
                err_count++;
            }
        }
    }
    if (err_count == 0) {
        printf(GREEN "GPIO %2d [%s] OK!\n", (int)gpio_rec.this_pin, pin_name_lookup.at(gpio_rec.this_pin).c_str());
    } else {
        printf(RED "GPIO %2d [%s] FAILED - %d errors!\n\r", (int)gpio_rec.this_pin,
               pin_name_lookup.at(gpio_rec.this_pin).c_str(), err_count);
    }
    return err_count;
}

void run_loopback_test()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    init_loopback();
    loopback_setup();


//     int sleep_time = 500000;
//     while(1){

//     LOGINFO("Setting all dir to out");
//     set_dtd_out();
//     set_ind_out();
//     set_tad_out();

//     usleep(sleep_time);
//     LOGINFO("ON")
//     for (auto cur_gpio : loopback_conn_table) {
//         bus->SetSignal(cur_gpio.this_pin, ON);
//         usleep(sleep_time);
//     }

//     LOGINFO("OFF");
//     for (auto cur_gpio : loopback_conn_table) {
//         bus->SetSignal(cur_gpio.this_pin, OFF);
//         usleep(sleep_time);
//     }

//     usleep(5000000);

//     LOGINFO("Setting all dir to IN");
//     set_dtd_in();
//     set_ind_in();
//     set_tad_in();

//     usleep(sleep_time);
//     LOGINFO("ON")
//     for (auto cur_gpio : loopback_conn_table) {
//         bus->SetSignal(cur_gpio.this_pin, ON);
//         usleep(sleep_time);
//     }

//     LOGINFO("OFF");
//     for (auto cur_gpio : loopback_conn_table) {
//         bus->SetSignal(cur_gpio.this_pin, OFF);
//         usleep(sleep_time);
//     }

// }



    for (auto cur_gpio : loopback_conn_table) {
        printf(CYAN "Testing GPIO %2d [%s]:" WHITE, (int)cur_gpio.this_pin,
               pin_name_lookup.at(cur_gpio.this_pin).c_str());
        test_gpio_pin(cur_gpio);
        // exit(1);
    }
}