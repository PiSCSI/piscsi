
// Kernel module to access cycle count registers:
//       https://matthewarcus.wordpress.com/2018/01/27/using-the-cycle-counter-registers-on-the-raspberry-pi-3/

// https://mindplusplus.wordpress.com/2013/05/21/accessing-the-raspberry-pis-1mhz-timer/

// Reading register from user space:
//      https://stackoverflow.com/questions/59749160/reading-from-register-of-allwinner-h3-arm-processor

// Maybe kernel patch>
// https://yhbt.net/lore/all/20140707085858.GG16262@lukather/T/

//
// Access the Raspberry Pi System Timer registers directly.
//
#include "hal/sbc_version.h"
#include "hal/systimer.h"
#include "rascsi_version.h"
#include "rasutil.h"
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <ostream>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
// #include "common.h"
#include "log.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "hal/gpiobus.h"
#include "hal/gpiobus_factory.h"

using namespace std;
using namespace spdlog;
//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------
static const int DEFAULT_PORT         = 6868;
static const char COMPONENT_SEPARATOR = ':';

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
static volatile bool active; // Processing flag
// RascsiService service;
shared_ptr<GPIOBUS> bus;
string current_log_level = "debug"; // Some versions of spdlog do not support get_log_level()
string access_token;
// DeviceFactory device_factory;
// shared_ptr<ControllerManager> controller_manager;
// RascsiImage rascsi_image;
// shared_ptr<RascsiResponse> rascsi_response;
// shared_ptr<RascsiExecutor> executor;
// const ProtobufSerializer serializer;

string connection_type = "hard-coded fullspec";

void Banner(int argc, char *argv[])
{
    cout << ras_util::Banner("Reloaded");
    cout << "Connect type: " << connection_type << '\n' << flush;

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

    // GPIOBUS creation
    bus = GPIOBUS_Factory::Create();

    // controller_manager = make_shared<ControllerManager>(bus);
    // rascsi_response = make_shared<RascsiResponse>(device_factory, *controller_manager, ScsiController::LUN_MAX);
    // executor  = make_shared<RascsiExecutor>(*rascsi_response, rascsi_image, device_factory, *controller_manager);

    // GPIO Initialization
    if (!bus->Init(BUS::mode_e::TARGET, board_type::rascsi_board_type_e::BOARD_TYPE_FULLSPEC)) {
        return false;
    }

    bus->Reset();

    return true;
}

void Cleanup()
{
    // executor->DetachAll();

    // service.Cleanup();

    bus->Cleanup();
}

void Reset()
{
    // controller_manager->ResetAllControllers();

    bus->Reset();
}

// bool InitBus()
// {
// 	LOGTRACE("%s", __PRETTY_FUNCTION__);
// 	SBC_Version::Init();

// 	return true;
// }

void test_timer();
void test_gpio();
void run_loopback_test();
void set_dtd_out();
void set_dtd_in();
void set_ind_out();
void set_ind_in();
void set_tad_out();
void set_tad_in();

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
    cout << "log level " << endl;
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
    // Cleanup();

    exit(signum);
}

#include "wiringPi.h"

void blink()
{
    printf("Simple Blink\n");
    // wiringPiSetup();
    // for(int i=0; i<20; i++){
    // pinMode(i,OUTPUT);
    // }

    // for(int i=0; i<20; i++){
    // 	for(int pin=0; pin<20; pin++){
    // 		printf("Setting pin %d high\n", pin);
    // 		digitalWrite(pin, HIGH);
    // 		delay(100);
    // 	}
    // 	for(int pin=0; pin<20; pin++){
    // 		printf("Setting pin %d low\n", pin);
    // 		digitalWrite(pin, LOW);
    // 		delay(100);
    // 	}
    // }
}

bool ParseArgument(int argc, char *argv[])
{
    // PbCommand command;
    // int id = -1;
    // int unit = -1;
    // PbDeviceType type = UNDEFINED;
    // int block_size = 0;
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
            // // The two options below are kind of a compound option with two letters
            // case 'i':
            // case 'I':
            // 	id = -1;
            // 	unit = -1;
            // 	continue;

            // case 'd':
            // case 'D': {
            // 	if (!ProcessId(optarg, id, unit)) {
            // 		return false;
            // 	}
            // 	continue;
            // }

            // case 'b': {
            // 	if (!GetAsInt(optarg, block_size)) {
            // 		cerr << "Invalid block size " << optarg << endl;
            // 		return false;
            // 	}
            // 	continue;
            // }

        case 'z':
            locale = optarg;
            continue;

            // case 'F': {
            // 	if (const string result = rascsi_image.SetDefaultFolder(optarg); !result.empty()) {
            // 		cerr << result << endl;
            // 		return false;
            // 	}
            // 	continue;
            // }

        case 'L':
            log_level = optarg;
            continue;

            // case 'R':
            // 	int depth;
            // 	if (!GetAsInt(optarg, depth) || depth < 0) {
            // 		cerr << "Invalid image file scan depth " << optarg << endl;
            // 		return false;
            // 	}
            // 	rascsi_image.SetDepth(depth);
            // 	continue;

        case 'n':
            name = optarg;
            continue;

            // case 'p':
            // 	if (!GetAsInt(optarg, port) || port <= 0 || port > 65535) {
            // 		cerr << "Invalid port " << optarg << ", port must be between 1 and 65535" << endl;
            // 		return false;
            // 	}
            // 	continue;

            // case 'P':
            // 	if (!ReadAccessToken(optarg)) {
            // 		return false;
            // 	}
            // 	continue;

            // case 'r': {
            // 		string error = executor->SetReservedIds(optarg);
            // 		if (!error.empty()) {
            // 			cerr << error << endl;
            // 			return false;
            // 		}
            // 	}
            // 	continue;

            // case 't': {
            // 		string t = optarg;
            // 		transform(t.begin(), t.end(), t.begin(), ::toupper);
            // 		if (!PbDeviceType_Parse(t, &type)) {
            // 			cerr << "Illegal device type '" << optarg << "'" << endl;
            // 			return false;
            // 		}
            // 	}
            // 	continue;

        case 1:
            // Encountered filename
            break;

        default:
            return false;
        }

        if (optopt) {
            return false;
        }

        // // Set up the device data
        // PbDeviceDefinition *device = command.add_devices();
        // device->set_id(id);
        // device->set_unit(unit);
        // device->set_type(type);
        // device->set_block_size(block_size);

        // ParseParameters(*device, optarg);

        // if (size_t separator_pos = name.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
        // 	device->set_vendor(name.substr(0, separator_pos));
        // 	name = name.substr(separator_pos + 1);
        // 	separator_pos = name.find(COMPONENT_SEPARATOR);
        // 	if (separator_pos != string::npos) {
        // 		device->set_product(name.substr(0, separator_pos));
        // 		device->set_revision(name.substr(separator_pos + 1));
        // 	}
        // 	else {
        // 		device->set_product(name);
        // 	}
        // }
        // else {
        // 	device->set_vendor(name);
        // }

        // id = -1;
        // type = UNDEFINED;
        // block_size = 0;
        // name = "";
    }

    if (!log_level.empty()) {
        SetLogLevel(log_level);
    }

    // // Attach all specified devices
    // command.set_operation(ATTACH);

    // if (CommandContext context(locale); !executor->ProcessCmd(context, command)) {
    // 	return false;
    // }

    // // Display and log the device list
    // PbServerInfo server_info;
    // rascsi_response->GetDevices(server_info, rascsi_image.GetDefaultFolder());
    // const list<PbDevice>& devices = { server_info.devices_info().devices().begin(),
    // server_info.devices_info().devices().end() }; const string device_list = ListDevices(devices);
    // LogDevices(device_list);
    // cout << device_list << flush;

    return true;
}

void tony_test();
void print_all();

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
    set_level(level::trace);

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

    LOGWARN("THIS WILL DO SOMETHING SOMEDAY");

    LOGDEBUG("DTD OUT %d = %d", (int)bus->board->pin_dtd, (int)bus->board->DtdOut());
    LOGDEBUG("DTD IN %d = %d", (int)bus->board->pin_dtd, (int)bus->board->DtdIn());
    LOGDEBUG("IND OUT %d = %d", (int)bus->board->pin_ind, (int)bus->board->IndOut());
    LOGDEBUG("IND IN %d = %d", (int)bus->board->pin_ind, (int)bus->board->IndIn());
    LOGDEBUG("TAD OUT %d = %d", (int)bus->board->pin_tad, (int)bus->board->TadOut());
    LOGDEBUG("TAD IN %d = %d", (int)bus->board->pin_tad, (int)bus->board->TadIn());

    bus->PullConfig(bus->board->pin_dtd, board_type::gpio_pull_up_down_e::GPIO_PULLUP);
    bus->PullConfig(bus->board->pin_ind, board_type::gpio_pull_up_down_e::GPIO_PULLUP);
    bus->PullConfig(bus->board->pin_tad, board_type::gpio_pull_up_down_e::GPIO_PULLUP);

    bus->PinConfig(bus->board->pin_dtd, board_type::gpio_direction_e::GPIO_OUTPUT);
    bus->PinConfig(bus->board->pin_ind, board_type::gpio_direction_e::GPIO_OUTPUT);
    bus->PinConfig(bus->board->pin_tad, board_type::gpio_direction_e::GPIO_OUTPUT);
    bus->PinConfig(bus->board->pin_ack, board_type::gpio_direction_e::GPIO_OUTPUT);

    int delay_time = 10000;
	(void)delay_time;

    set_tad_out();
    set_dtd_out();
    set_ind_out();






    // for (int i = 0; i < 10; i++) {
    //     bus->SetACK(true);
    //     usleep(delay_time);
    //     bus->SetACK(false);
    //     usleep(delay_time);
    // }

    run_loopback_test();
    return 0;

    tony_test();

    LOGDEBUG("bus->Acquire(): %08X", bus->Acquire());
    SysTimer::SleepUsec(1000);

    LOGDEBUG("IO True");
    bus->SetIO(true);
    SysTimer::SleepUsec(1000);
    LOGDEBUG("bus->Acquire(): %08X", bus->Acquire());

    LOGDEBUG("IO False");
    bus->SetIO(false);
    SysTimer::SleepUsec(1000);
    LOGDEBUG("bus->Acquire(): %08X", bus->Acquire());

    return 0;
}

// //---------------------------------------------------------------------------
// //
// //	Main processing
// //
// //---------------------------------------------------------------------------
// int main(int argc, char* argv[])
// {

// 	// added setvbuf to override stdout buffering, so logs are written immediately and not when the process exits.
// 	setvbuf(stdout, nullptr, _IONBF, 0);

// 	if(argc > 1){
// 		SetLogLevel(argv[1]);
// 	}else{
// 		SetLogLevel("trace");
// 	}

// 	// Create a thread-safe stdout logger to process the log messages
// 	auto logger = spdlog::stdout_color_mt("rascsi stdout logger");

// 	// Signal handler to detach all devices on a KILL or TERM signal
// 	struct sigaction termination_handler;
// 	termination_handler.sa_handler = TerminationHandler;
// 	sigemptyset(&termination_handler.sa_mask);
// 	termination_handler.sa_flags = 0;
// 	sigaction(SIGINT, &termination_handler, nullptr);
// 	sigaction(SIGTERM, &termination_handler, nullptr);

// 	if (!InitBus()) {
// 		return EPERM;
// 	}

// 	// blink();
// 	// test_timer();
// 	test_gpio();

// }

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

void tony_test()
{
    LOGWARN("Set everything to HIGH OUTPUT with PULLUP");

    for (auto phys_pin : bus->SignalTable) {
        LOGTRACE("%s GPIO_PULLUP", __PRETTY_FUNCTION__);

        bus->PullConfig(phys_pin, board_type::gpio_pull_up_down_e::GPIO_PULLUP);
        bus->PinConfig(phys_pin, board_type::gpio_direction_e::GPIO_OUTPUT);
        bus->PinSetSignal(phys_pin, board_type::gpio_high_low_e::GPIO_STATE_HIGH);
        SysTimer::SleepUsec(10000);
    }
    // SysTimer::SleepUsec(1000);
    usleep(1000000);

    LOGWARN("Set everything to LOW OUTPUT with PULLUP");

    for (auto phys_pin : bus->SignalTable) {
        LOGTRACE("%s GPIO_PULLUP", __PRETTY_FUNCTION__);

        bus->PullConfig(phys_pin, board_type::gpio_pull_up_down_e::GPIO_PULLUP);
        bus->PinConfig(phys_pin, board_type::gpio_direction_e::GPIO_OUTPUT);
        bus->PinSetSignal(phys_pin, board_type::gpio_high_low_e::GPIO_STATE_LOW);
        SysTimer::SleepUsec(10000);
    }
    usleep(1000000);

    LOGWARN("Set everything to INPUT");

    for (auto phys_pin : bus->SignalTable) {
        LOGTRACE("%s GPIO_PULLUP", __PRETTY_FUNCTION__);

        bus->PullConfig(phys_pin, board_type::gpio_pull_up_down_e::GPIO_PULLUP);
        bus->PinConfig(phys_pin, board_type::gpio_direction_e::GPIO_INPUT);
        bus->PinSetSignal(phys_pin, board_type::gpio_high_low_e::GPIO_STATE_LOW);
        SysTimer::SleepUsec(10000);
    }
    usleep(1000000);
}

struct loopback_connections_struct {
    board_type::pi_physical_pin_e this_pin;
    board_type::pi_physical_pin_e connected_pin;
    board_type::pi_physical_pin_e dir_ctrl_pin;
};
typedef loopback_connections_struct loopback_connection;

std::map<board_type::pi_physical_pin_e, string> pin_name_lookup;
std::vector<loopback_connection> loopback_conn_table;

// This needs to run AFTER GPIOBUS has been initialized. Otherwise, we don't know what type of board
// we're using
void init_loopback()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    shared_ptr<board_type::Rascsi_Board_Type> board_table = bus->board;

    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_dt0, .connected_pin = board_table->pin_ack, .dir_ctrl_pin = board_table->pin_dtd});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_dt1, .connected_pin = board_table->pin_sel, .dir_ctrl_pin = board_table->pin_dtd});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_dt2, .connected_pin = board_table->pin_atn, .dir_ctrl_pin = board_table->pin_dtd});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_dt3, .connected_pin = board_table->pin_rst, .dir_ctrl_pin = board_table->pin_dtd});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_dt4, .connected_pin = board_table->pin_cd, .dir_ctrl_pin = board_table->pin_dtd});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_dt5, .connected_pin = board_table->pin_io, .dir_ctrl_pin = board_table->pin_dtd});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_dt6, .connected_pin = board_table->pin_msg, .dir_ctrl_pin = board_table->pin_dtd});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_dt7, .connected_pin = board_table->pin_req, .dir_ctrl_pin = board_table->pin_dtd});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_dp, .connected_pin = board_table->pin_bsy, .dir_ctrl_pin = board_table->pin_dtd});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_atn, .connected_pin = board_table->pin_dt2, .dir_ctrl_pin = board_table->pin_ind});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_rst, .connected_pin = board_table->pin_dt3, .dir_ctrl_pin = board_table->pin_ind});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_ack, .connected_pin = board_table->pin_dt0, .dir_ctrl_pin = board_table->pin_ind});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_req, .connected_pin = board_table->pin_dt7, .dir_ctrl_pin = board_table->pin_tad});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_msg, .connected_pin = board_table->pin_dt6, .dir_ctrl_pin = board_table->pin_tad});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_cd, .connected_pin = board_table->pin_dt4, .dir_ctrl_pin = board_table->pin_tad});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_io, .connected_pin = board_table->pin_dt5, .dir_ctrl_pin = board_table->pin_tad});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_bsy, .connected_pin = board_table->pin_dp, .dir_ctrl_pin = board_table->pin_tad});
    loopback_conn_table.push_back(loopback_connection{
        .this_pin = board_table->pin_sel, .connected_pin = board_table->pin_dt1, .dir_ctrl_pin = board_table->pin_ind});

    pin_name_lookup[board_table->pin_dt0]                            = " d0";
    pin_name_lookup[board_table->pin_dt1]                            = " d1";
    pin_name_lookup[board_table->pin_dt2]                            = " d2";
    pin_name_lookup[board_table->pin_dt3]                            = " d3";
    pin_name_lookup[board_table->pin_dt4]                            = " d4";
    pin_name_lookup[board_table->pin_dt5]                            = " d5";
    pin_name_lookup[board_table->pin_dt6]                            = " d6";
    pin_name_lookup[board_table->pin_dt7]                            = " d7";
    pin_name_lookup[board_table->pin_dp]                             = " dp";
    pin_name_lookup[board_table->pin_atn]                            = "atn";
    pin_name_lookup[board_table->pin_rst]                            = "rst";
    pin_name_lookup[board_table->pin_ack]                            = "ack";
    pin_name_lookup[board_table->pin_req]                            = "req";
    pin_name_lookup[board_table->pin_msg]                            = "msg";
    pin_name_lookup[board_table->pin_cd]                             = " cd";
    pin_name_lookup[board_table->pin_io]                             = " io";
    pin_name_lookup[board_table->pin_bsy]                            = "bsy";
    pin_name_lookup[board_table->pin_sel]                            = "sel";
    pin_name_lookup[board_table->pin_ind]                            = "ind";
    pin_name_lookup[board_table->pin_tad]                            = "tad";
    pin_name_lookup[board_table->pin_dtd]                            = "dtd";
    pin_name_lookup[board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE] = "NONE";
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
    LOGDEBUG("DTD OUT %d = %d", (int)bus->board->pin_dtd, (int)bus->board->DtdOut());
    bus->PinSetSignal(bus->board->pin_dtd, bus->board->DtdOut());
    // gpio.output(rascsi_dtd_gpio,gpio.LOW)
}

// Set transceivers IC1 and IC2 to INPUT
void set_dtd_in()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    LOGDEBUG("DTD IN %d = %d", (int)bus->board->pin_dtd, (int)bus->board->DtdIn());
    bus->PinSetSignal(bus->board->pin_dtd, bus->board->DtdIn());
    // gpio.output(rascsi_dtd_gpio,gpio.HIGH)
}
// Set transceiver IC4 to OUTPUT
void set_ind_out()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    LOGDEBUG("IND OUT %d = %d", (int)bus->board->pin_ind, (int)bus->board->IndOut());
    bus->PinSetSignal(bus->board->pin_ind, bus->board->IndOut());
    // gpio.output(rascsi_ind_gpio,gpio.HIGH)
}
// Set transceiver IC4 to INPUT
void set_ind_in()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    LOGDEBUG("IND IN %d = %d", (int)bus->board->pin_ind, (int)bus->board->IndIn());
    bus->PinSetSignal(bus->board->pin_ind, bus->board->IndIn());
    // gpio.output(rascsi_ind_gpio,gpio.LOW)
}
// Set transceiver IC3 to OUTPUT
void set_tad_out()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    LOGDEBUG("TAD OUT %d = %d", (int)bus->board->pin_tad, (int)bus->board->TadOut());
    bus->PinSetSignal(bus->board->pin_tad, bus->board->TadOut());
    // gpio.output(rascsi_tad_gpio,gpio.HIGH)
}
// Set transceiver IC3 to INPUT
void set_tad_in()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    LOGDEBUG("TAD IN %d = %d", (int)bus->board->pin_tad, (int)bus->board->TadIn());
    bus->PinSetSignal(bus->board->pin_tad, bus->board->TadIn());
    // gpio.output(rascsi_tad_gpio,gpio.LOW)
}

// Set the specified transciever to an OUTPUT. All of the other transceivers
// will be set to inputs. If a non-existent direction gpio is specified, this
// will set all of the transceivers to inputs.
void set_output_channel(board_type::pi_physical_pin_e out_gpio)
{
    LOGTRACE("%s tad: %d dtd: %d ind: %d", bus->board->connect_desc.c_str(), (int)bus->board->pin_tad,
             (int)bus->board->pin_dtd, (int)bus->board->pin_ind);
    if (out_gpio == bus->board->pin_tad)
        set_tad_out();
    else
        set_tad_in();
    if (out_gpio == bus->board->pin_dtd)
        set_dtd_out();
    else
        set_dtd_in();
    if (out_gpio == bus->board->pin_ind)
        set_ind_out();
    else
        set_ind_in();
}

// Initialize the GPIO library, set all of the gpios associated with SCSI signals to outputs and set
// all of the direction control gpios to outputs
void loopback_setup()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    // gpio.setmode(gpio.BOARD)
    // gpio.setwarnings(False)
    for (loopback_connection cur_gpio : loopback_conn_table) {
        if (cur_gpio.this_pin == board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE) {
            continue;
        }
        bus->PinConfig(cur_gpio.this_pin, board_type::gpio_direction_e::GPIO_OUTPUT);
        bus->PullConfig(cur_gpio.this_pin, board_type::gpio_pull_up_down_e::GPIO_PULLNONE);
        // gpio.setup(cur_gpio['gpio_num'], gpio.OUT, initial=gpio.HIGH)
    }
    // Setup direction control
    // gpio.setup(rascsi_ind_gpio, gpio.OUT) gpio.setup(rascsi_tad_gpio, gpio.OUT) gpio.setup(rascsi_dtd_gpio, gpio.OUT)

    if (bus->board->pin_ind != board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE) {
        bus->PinConfig(bus->board->pin_ind, board_type::gpio_direction_e::GPIO_OUTPUT);
    }
    if (bus->board->pin_tad != board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE) {
        bus->PinConfig(bus->board->pin_tad, board_type::gpio_direction_e::GPIO_OUTPUT);
    }
    if (bus->board->pin_dtd != board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE) {
        bus->PinConfig(bus->board->pin_dtd, board_type::gpio_direction_e::GPIO_OUTPUT);
    }
}

// Main test procedure.This will execute for each of the SCSI pins to make sure its connected
// properly.
int test_gpio_pin(loopback_connection &gpio_rec)
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    (void)gpio_rec;
    int err_count  = 0;
    int sleep_time = 1000000;

    LOGDEBUG("dir ctrl pin: %d", (int)gpio_rec.dir_ctrl_pin);
    set_output_channel(gpio_rec.dir_ctrl_pin);
	usleep(sleep_time);

    // Set all GPIOs high (initialize to a known state)
    for (auto cur_gpio : loopback_conn_table) {
        bus->PinSetSignal(cur_gpio.this_pin, board_type::gpio_high_low_e::GPIO_STATE_HIGH);
    }

	usleep(sleep_time);
	    bus->Acquire();

    // ############################################
    // # set the test gpio low
    // gpio.output(gpio_rec['gpio_num'], gpio.LOW)
    LOGTRACE("PinSetSignal %d", (int)gpio_rec.this_pin);
    bus->PinSetSignal(gpio_rec.this_pin, board_type::gpio_high_low_e::GPIO_STATE_LOW);

    // time.sleep(pin_settle_delay)
    LOGINFO("Sleep");
    usleep(sleep_time);
    LOGINFO("Done");

    LOGTRACE("Acquire");
    bus->Acquire();
    // # loop through all of the gpios
    for (auto cur_gpio : loopback_conn_table) {
        // all of the gpios should be high except for the test gpio and the connected gpio
		LOGTRACE("calling bus->GetSignal(%d)", (int)cur_gpio.this_pin);
        auto cur_val = bus->GetSignal(cur_gpio.this_pin);
        LOGDEBUG("%d [%s] is %d", (int)cur_gpio.this_pin, pin_name_lookup.at(cur_gpio.this_pin).c_str(), (int)cur_val);

        if (cur_gpio.this_pin == gpio_rec.this_pin) {
            if (cur_val != false) {
                LOGERROR("Test commanded GPIO %d [%s] to be low, but it did not respond", (int)cur_gpio.this_pin,
                         pin_name_lookup.at(cur_gpio.this_pin).c_str())
                err_count++;
            }
        } else if (cur_gpio.this_pin == gpio_rec.connected_pin) {
            if (cur_val != false) {
                LOGERROR("GPIO %d [%s] should be driven low, but %d [%s] did not affect it", (int)cur_gpio.this_pin,
                         pin_name_lookup.at(cur_gpio.this_pin).c_str(), (int)cur_gpio.connected_pin,
                         pin_name_lookup.at(cur_gpio.connected_pin).c_str());

                err_count++;
            }
        } else {
            if (cur_val != true) {
                LOGERROR("GPIO %d [%s] was incorrectly pulled low, when it shouldn't be", (int)cur_gpio.this_pin,
                         pin_name_lookup.at(cur_gpio.this_pin).c_str())
                err_count++;
            }
        }
    }
	exit(1);


    //         if(cur_val != gpio.HIGH):
    //             print("Error: GPIO " + scsi_signals[gpio_rec['gpio_num']] + " incorrectly pulled " +
    //             scsi_signals[cur_gpio] + " LOW, when it shouldn't have") err_count = err_count+1

    // ############################################
    // # set the transceivers to input
    // set_output_channel(rascsi_none)

    // time.sleep(pin_settle_delay)

    // # loop through all of the gpios
    // for cur_gpio in scsi_signals:
    //     # all of the gpios should be high except for the test gpio
    //     cur_val = gpio.input(cur_gpio)
    //     if( cur_gpio == gpio_rec['gpio_num']):
    //         if(cur_val != gpio.LOW):
    //             print("Error: Test commanded GPIO " + scsi_signals[gpio_rec['gpio_num']] + " to be
    //             low, but it did not respond") err_count = err_count+1
    //     else:
    //         if(cur_val != gpio.HIGH):
    //             print("Error: GPIO " + scsi_signals[gpio_rec['gpio_num']] + " incorrectly pulled " +
    //             scsi_signals[cur_gpio] + " LOW, when it shouldn't have") err_count = err_count+1

    // # Set the transceiver back to output
    // set_output_channel(gpio_rec['dir_ctrl'])

    // #############################################
    // # set the test gpio high
    // gpio.output(gpio_rec['gpio_num'], gpio.HIGH)

    // time.sleep(pin_settle_delay)

    // # loop through all of the gpios
    // for cur_gpio in scsi_signals:
    //     # all of the gpios should be high
    //     cur_val = gpio.input(cur_gpio)
    //     if( cur_gpio == gpio_rec['gpio_num']):
    //         if(cur_val != gpio.HIGH):
    //             print("Error: Test commanded GPIO " + scsi_signals[gpio_rec['gpio_num']] + " to be
    //             high, but it did not respond") err_count = err_count+1
    //     else:
    //         if(cur_val != gpio.HIGH):
    //             print("Error: GPIO " + scsi_signals[gpio_rec['gpio_num']] + " incorrectly pulled " +
    //             scsi_signals[cur_gpio] + " LOW, when it shouldn't have") err_count = err_count+1
    return err_count;
}

void run_loopback_test()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    init_loopback();
    loopback_setup();
    LOGWARN("DONE WITH LOOPBACK_SETUP()");
    print_all();
    LOGWARN("---------------------------------------------------");

	set_output_channel(bus->board->pin_dtd);
	for(int j = 0; j<5; j++){
		for(uint8_t i = 0; i< 0xFF; i++){
			bus->SetDAT(i);
			usleep(100000);
		}
	}


	loopback_connection ack;
	ack.this_pin = bus->board->pin_ack;
	ack.connected_pin = bus->board->pin_dt0;
	ack.dir_ctrl_pin = bus->board->pin_ind;
	
    test_gpio_pin(ack);
        

    for (auto cur_gpio : loopback_conn_table) {
        LOGINFO("Testing GPIO %d [%s]..................................", (int)cur_gpio.this_pin,
                pin_name_lookup.at(cur_gpio.this_pin).c_str());
        test_gpio_pin(cur_gpio);
    }
}