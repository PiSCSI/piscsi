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
#include "filepath.h"
#include "fileio.h"
#include "log.h"
#include "gpiobus.h"
#include "rascsi_version.h"
#include "spdlog/spdlog.h"
#include <sys/time.h>
#include <climits>
#include <sstream>
#include <iostream>
#include "rascsi.h"

using namespace std;

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------
#define MAX_BUFF_SIZE 1000000

// Symbol definition for the VCD file
// These are just arbitrary symbols. They can be anything allowed by the VCD file format,
// as long as they're consistently used.
#define	SYMBOL_PIN_DAT '#'
#define	SYMBOL_PIN_ATN '+'
#define	SYMBOL_PIN_RST '$'
#define	SYMBOL_PIN_ACK '%'
#define	SYMBOL_PIN_REQ '^'
#define	SYMBOL_PIN_MSG '&'
#define	SYMBOL_PIN_CD  '*'
#define	SYMBOL_PIN_IO  '('
#define	SYMBOL_PIN_BSY ')'
#define	SYMBOL_PIN_SEL '-'
#define SYMBOL_PIN_PHASE '='

// We'll use position 0 in the prev_value array to store the previous phase
#define PIN_PHASE 0

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
static BYTE prev_value[32] = {0xFF};
static volatile bool running;		// Running flag
GPIOBUS *bus;						// GPIO Bus
typedef struct data_capture{
    DWORD data;
    uint64_t timestamp;
} data_capture_t;

data_capture data_buffer[MAX_BUFF_SIZE];
DWORD data_idx = 0;

double ns_per_loop;

// We don't really need to support 256 character file names - this causes
// all kinds of compiler warnings when the log filename can be up to 256
// characters. _MAX_FNAME/2 is just an arbitrary value.
char log_file_name[_MAX_FNAME/2] = "log.vcd";

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

//---------------------------------------------------------------------------
//
//	Banner Output
//
//---------------------------------------------------------------------------
void Banner(int argc, char* argv[])
{
	LOGINFO("SCSI Monitor Capture Tool - part of RaSCSI(*^..^*) ");
	LOGINFO("version %s (%s, %s)\n",
		rascsi_get_version_string(),
		__DATE__,
		__TIME__);
	LOGINFO("Powered by XM6 TypeG Technology ");
	LOGINFO("Copyright (C) 2016-2020 GIMONS");
	LOGINFO("Copyright (C) 2020-2021 Contributors to the RaSCSI project");
	LOGINFO("Connect type : %s", CONNECT_DESC);
	LOGINFO("   %s - Value Change Dump file that can be opened with GTKWave", log_file_name);

	if ((argc > 1 && strcmp(argv[1], "-h") == 0) ||
		(argc > 1 && strcmp(argv[1], "--help") == 0)){
		LOGINFO("Usage: %s [log filename]...", argv[0]);
		exit(0);
	}
    else
    {
        LOGINFO(" ");
        LOGINFO("Now collecting data.... Press CTRL-C to stop.")
        LOGINFO(" ");
    }
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
bool Init()
{
	// Interrupt handler settings
	if (signal(SIGINT, KillHandler) == SIG_ERR) {
		return FALSE;
	}
	if (signal(SIGHUP, KillHandler) == SIG_ERR) {
		return FALSE;
	}
	if (signal(SIGTERM, KillHandler) == SIG_ERR) {
		return FALSE;
	}

	// GPIO Initialization
	bus = new GPIOBUS();
	if (!bus->Init()) {
        LOGERROR("Unable to intiailize the GPIO bus. Exiting....");
		return false;
	}

	// Bus Reset
	bus->Reset();

	// Other
	running = false;

	return true;
}

BOOL get_pin_value(DWORD data, int pin)
{
	return (data >> pin) & 1;
}

BYTE get_data_field(DWORD data)
{
	DWORD data_out =
		((data >> (PIN_DT0 - 0)) & (1 << 7)) |
		((data >> (PIN_DT1 - 1)) & (1 << 6)) |
		((data >> (PIN_DT2 - 2)) & (1 << 5)) |
		((data >> (PIN_DT3 - 3)) & (1 << 4)) |
		((data >> (PIN_DT4 - 4)) & (1 << 3)) |
		((data >> (PIN_DT5 - 5)) & (1 << 2)) |
		((data >> (PIN_DT6 - 6)) & (1 << 1)) |
		((data >> (PIN_DT7 - 7)) & (1 << 0));

	return (BYTE)data_out;
}

void vcd_output_if_changed_phase(FILE *fp, DWORD data, int pin, char symbol)
{
    BUS::phase_t new_value = GPIOBUS::GetPhaseRaw(data);
    if (prev_value[pin] != new_value)
    {
        prev_value[pin] = new_value;
        fprintf(fp, "s%s %c\n", GPIOBUS::GetPhaseStrRaw(new_value), symbol);
    }
}

void vcd_output_if_changed_bool(FILE *fp, DWORD data, int pin, char symbol)
{
    BOOL new_value = get_pin_value(data,pin);
    if (prev_value[pin] != new_value)
    {
        prev_value[pin] = new_value;
        fprintf(fp, "%d%c\n", new_value, symbol);
    }
}

void vcd_output_if_changed_byte(FILE *fp, DWORD data, int pin, char symbol)
{
    BYTE new_value = get_data_field(data);
    if(prev_value[pin] != new_value)
    {
        prev_value[pin] = new_value;
        fprintf(fp, "b%d%d%d%d%d%d%d%d %c\n",
        get_pin_value(data,PIN_DT7),
        get_pin_value(data,PIN_DT6),
        get_pin_value(data,PIN_DT5),
        get_pin_value(data,PIN_DT4),
        get_pin_value(data,PIN_DT3),
        get_pin_value(data,PIN_DT2),
        get_pin_value(data,PIN_DT1),
        get_pin_value(data,PIN_DT0), symbol);
    }
}

void create_value_change_dump()
{
    LOGINFO("Creating Value Change Dump file (%s)", log_file_name);
    FILE *fp = fopen(log_file_name,"w");

    // Get the current time
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    char timestamp[256];
    strftime (timestamp,sizeof(timestamp),"%d-%m-%Y %H-%M-%S",timeinfo);

    fprintf(fp, "$date\n");
    fprintf(fp, "%s\n", timestamp);
    fprintf(fp, "$end\n");
    fprintf(fp, "$version\n");
    fprintf(fp, "   VCD generator tool version info text.\n");
    fprintf(fp, "$end\n");
    fprintf(fp, "$comment\n");
    fprintf(fp, "   Tool build date:%s\n", __TIMESTAMP__);
    fprintf(fp, "$end\n");
    fprintf(fp, "$timescale 1 ns $end\n");
    fprintf(fp, "$scope module logic $end\n");
    fprintf(fp, "$var wire 1 %c BSY $end\n", SYMBOL_PIN_BSY);
    fprintf(fp, "$var wire 1 %c SEL $end\n", SYMBOL_PIN_SEL);
    fprintf(fp, "$var wire 1 %c CD $end\n",  SYMBOL_PIN_CD);
    fprintf(fp, "$var wire 1 %c IO $end\n",  SYMBOL_PIN_IO);
    fprintf(fp, "$var wire 1 %c MSG $end\n", SYMBOL_PIN_MSG);
    fprintf(fp, "$var wire 1 %c REQ $end\n", SYMBOL_PIN_REQ);
    fprintf(fp, "$var wire 1 %c ACK $end\n", SYMBOL_PIN_ACK);
    fprintf(fp, "$var wire 1 %c ATN $end\n", SYMBOL_PIN_ATN);
    fprintf(fp, "$var wire 1 %c RST $end\n", SYMBOL_PIN_RST);
    fprintf(fp, "$var wire 8 %c data $end\n", SYMBOL_PIN_DAT);
    fprintf(fp, "$var string 1 %c phase $end\n", SYMBOL_PIN_PHASE);
    fprintf(fp, "$upscope $end\n");
    fprintf(fp, "$enddefinitions $end\n");

    // Initial values - default to zeros
    fprintf(fp, "$dumpvars\n");
    fprintf(fp, "0%c\n", SYMBOL_PIN_BSY);
    fprintf(fp, "0%c\n", SYMBOL_PIN_SEL);
    fprintf(fp, "0%c\n",  SYMBOL_PIN_CD);
    fprintf(fp, "0%c\n",  SYMBOL_PIN_IO);
    fprintf(fp, "0%c\n", SYMBOL_PIN_MSG);
    fprintf(fp, "0%c\n", SYMBOL_PIN_REQ);
    fprintf(fp, "0%c\n", SYMBOL_PIN_ACK);
    fprintf(fp, "0%c\n", SYMBOL_PIN_ATN);
    fprintf(fp, "0%c\n", SYMBOL_PIN_RST);
    fprintf(fp, "b00000000 %c\n", SYMBOL_PIN_DAT);
    fprintf(fp, "$end\n");

    DWORD i = 0;
    while (i < data_idx)
    {
    	ostringstream s;
    	s << (uint64_t)(data_buffer[i].timestamp*ns_per_loop);
        fprintf(fp, "#%s\n",s.str().c_str());
        vcd_output_if_changed_bool(fp, data_buffer[i].data, PIN_BSY, SYMBOL_PIN_BSY);
        vcd_output_if_changed_bool(fp, data_buffer[i].data, PIN_SEL, SYMBOL_PIN_SEL);
        vcd_output_if_changed_bool(fp, data_buffer[i].data, PIN_CD,  SYMBOL_PIN_CD);
        vcd_output_if_changed_bool(fp, data_buffer[i].data, PIN_IO,  SYMBOL_PIN_IO);
        vcd_output_if_changed_bool(fp, data_buffer[i].data, PIN_MSG, SYMBOL_PIN_MSG);
        vcd_output_if_changed_bool(fp, data_buffer[i].data, PIN_REQ, SYMBOL_PIN_REQ);
        vcd_output_if_changed_bool(fp, data_buffer[i].data, PIN_ACK, SYMBOL_PIN_ACK);
        vcd_output_if_changed_bool(fp, data_buffer[i].data, PIN_ATN, SYMBOL_PIN_ATN);
        vcd_output_if_changed_bool(fp, data_buffer[i].data, PIN_RST, SYMBOL_PIN_RST);
        vcd_output_if_changed_byte(fp, data_buffer[i].data, PIN_DT0, SYMBOL_PIN_DAT);
        vcd_output_if_changed_phase(fp, data_buffer[i].data, PIN_PHASE, SYMBOL_PIN_PHASE);
        i++;
    }
    fclose(fp);
}

void Cleanup()
{
    LOGINFO("Stopping data collection....");
    create_value_change_dump();

	// Cleanup the Bus
	bus->Cleanup();

	delete bus;
}

void Reset()
{
	// Reset the bus
	bus->Reset();
}

//---------------------------------------------------------------------------
//
//	Pin the thread to a specific CPU
//
//---------------------------------------------------------------------------
void FixCpu(int cpu)
{
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

#ifdef DEBUG
static DWORD high_bits = 0x0;
static DWORD low_bits = 0xFFFFFFFF;
#endif 

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    ostringstream s;

    #ifdef DEBUG
    DWORD prev_high = high_bits;
    DWORD prev_low = low_bits;
#endif
    DWORD prev_sample = 0xFFFFFFFF;
    DWORD this_sample = 0;
    struct sched_param schparam;
    timeval start_time;
    timeval stop_time;
    uint64_t loop_count = 0;
    timeval time_diff;
    uint64_t elapsed_us;
    int str_len;

    // If there is an argument specified and it is NOT -h or --help
    if((argc > 1) && (strcmp(argv[1], "-h")) && (strcmp(argv[1], "--help"))){
        str_len = strlen(argv[1]);
        if ((str_len >= 1) && (str_len < _MAX_FNAME))
        {
            strncpy(log_file_name, argv[1], sizeof(log_file_name));
            // Append .vcd if its not already there
            if((str_len < 4) || strcasecmp(log_file_name + (str_len - 4), ".vcd")) {
                strcat(log_file_name, ".vcd");
            }
        }
        else
        {
            printf("Invalid log name specified. Using log.vcd");
        }
    }

#ifdef DEBUG
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    spdlog::set_pattern("%^[%l]%$ %v");
	// Output the Banner
	Banner(argc, argv);
    memset(data_buffer,0,sizeof(data_buffer));

	// Initialize
	int ret = 0;
	if (!Init()) {
		ret = EPERM;
		goto init_exit;
	}

	// Reset
	Reset();

    // Set the affinity to a specific processor core
	FixCpu(3);

	// Scheduling policy setting (highest priority)
	schparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &schparam);

	// Start execution
	running = true;
	bus->SetACT(FALSE);

    (void)gettimeofday(&start_time, NULL);

    LOGDEBUG("ALL_SCSI_PINS %08X\n",ALL_SCSI_PINS);

    // Main Loop
	while (running) {
		// Work initialization
		this_sample = (bus->Aquire() & ALL_SCSI_PINS);
        loop_count++;
        if (loop_count > LLONG_MAX -1)
        {
            LOGINFO("Maximum amount of time has elapsed. SCSIMON is terminating.");
            running=false;
        }
        if (data_idx >= (MAX_BUFF_SIZE-2))
        {
            LOGINFO("Internal data buffer is full. SCSIMON is terminating.");
            running=false;
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
                LOGDEBUG("   %08X    %08X\n",high_bits, low_bits);
            }
            prev_high = high_bits;
            prev_low = low_bits;
            if((data_idx % 1000) == 0){
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
    if (data_idx < MAX_BUFF_SIZE)
    {
        data_buffer[data_idx].data = this_sample;
        data_buffer[data_idx].timestamp = loop_count;
        data_idx++;
    }

    (void)gettimeofday(&stop_time, NULL);

    timersub(&stop_time, &start_time, &time_diff);

    elapsed_us = ((time_diff.tv_sec*1000000) + time_diff.tv_usec);
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
