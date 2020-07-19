//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ SCSI Shark main]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"
#include "disk.h"
#include "gpiobus.h"
#include "spdlog/spdlog.h"
#include <sys/time.h>

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------
#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
static BYTE prev_value[32] = {0xFF};
static volatile BOOL running;		// Running flag
static volatile BOOL active;		// Processing flag
GPIOBUS *bus;						// GPIO Bus
typedef struct data_capture{
    DWORD data;
    timeval timestamp;
} data_capture_t;


#define MAX_BUFF_SIZE 1000000

data_capture data_buffer[MAX_BUFF_SIZE];
int data_idx = 0;

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


//---------------------------------------------------------------------------
//
//	Signal Processing
//
//---------------------------------------------------------------------------
void KillHandler(int sig)
{
	// Stop instruction
	running = FALSE;
}

//---------------------------------------------------------------------------
//
//	Banner Output
//
//---------------------------------------------------------------------------
void Banner(int argc, char* argv[])
{
	FPRT(stdout,"SCSI Shark Capture Tool - part of RaSCSI(*^..^*) ");
	FPRT(stdout,"version %01d.%01d%01d(%s, %s)\n",
		(int)((VERSION >> 8) & 0xf),
		(int)((VERSION >> 4) & 0xf),
		(int)((VERSION     ) & 0xf),
		__DATE__,
		__TIME__);
	FPRT(stdout,"Powered by XM6 TypeG Technology / ");
	FPRT(stdout,"Copyright (C) 2016-2020 GIMONS\n");
	FPRT(stdout,"Copyright (C) 2020 akuker\n");
	FPRT(stdout,"Connect type : %s\n", CONNECT_DESC);

	FPRT(stdout,"\nPress CTRL-C to stop collecting data.\n");
	FPRT(stdout,"   log.csv - CSV listing of the data collected\n");
	FPRT(stdout,"   log.vcd - Value Change Dump file that can be opened with GTKWave\n");

	if ((argc > 1 && strcmp(argv[1], "-h") == 0) ||
		(argc > 1 && strcmp(argv[1], "--help") == 0)){
		FPRT(stdout,"\n");
		FPRT(stdout,"Usage: %s ...\n\n", argv[0]);

		exit(0);
	}
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL Init()
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

	// GPIOBUS creation
	bus = new GPIOBUS();

	// GPIO Initialization
	if (!bus->Init()) {
		return FALSE;
	}

	// Bus Reset
	bus->Reset();

	// Other
	running = FALSE;
	active = FALSE;

	return TRUE;
}

BOOL get_pin_value(DWORD data, int pin)
{
	return  (data >> pin) & 1;
}

BYTE get_data_field(DWORD data)
{
	DWORD data_out;
	data_out =
		((data >> (PIN_DT0 - 0)) & (1 << 0)) |
		((data >> (PIN_DT1 - 1)) & (1 << 1)) |
		((data >> (PIN_DT2 - 2)) & (1 << 2)) |
		((data >> (PIN_DT3 - 3)) & (1 << 3)) |
		((data >> (PIN_DT4 - 4)) & (1 << 4)) |
		((data >> (PIN_DT5 - 5)) & (1 << 5)) |
		((data >> (PIN_DT6 - 6)) & (1 << 6)) |
		((data >> (PIN_DT7 - 7)) & (1 << 7));

	return (BYTE)data_out;
}


void vcd_output_if_changed_bool(FILE *fp, DWORD data, int pin, char symbol)
{
    BOOL new_value = get_pin_value(data,pin);
    if(prev_value[pin] != new_value)
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
        get_pin_value(data,PIN_DT0),
        get_pin_value(data,PIN_DT1),
        get_pin_value(data,PIN_DT2),
        get_pin_value(data,PIN_DT3),
        get_pin_value(data,PIN_DT4),
        get_pin_value(data,PIN_DT5),
        get_pin_value(data,PIN_DT6),
        get_pin_value(data,PIN_DT7), symbol);
    }
}


void create_value_change_dump()
{
    time_t rawtime;
    struct tm * timeinfo;
    int i = 0;
    timeval time_diff;
    char timestamp[256];
    FILE *fp;
    timeval start_time = data_buffer[0].timestamp;
    printf("Creating Value Change Dump file (log.vcd)\n");
    fp = fopen("log.vcd","w");

    // Get the current time
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    strftime (timestamp,sizeof(timestamp),"%d-%m-%Y %H-%M-%S",timeinfo);

    fprintf(fp, "$date\n");
    fprintf(fp, "%s\n", timestamp);
    fprintf(fp, "$end\n");
    fprintf(fp, "$version\n");
    fprintf(fp, "   VCD generator tool version info text.\n");
    fprintf(fp, "$end\n");
    fprintf(fp, "$comment\n");
    fprintf(fp, "   Any comment text.\n");
    fprintf(fp, "$end\n");
    fprintf(fp, "$timescale 1 us $end\n");
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

    while(i < data_idx)
    {
        timersub(&(data_buffer[i].timestamp), &start_time, &time_diff);
        fprintf(fp, "#%ld\n",((time_diff.tv_sec*1000000) + time_diff.tv_usec));
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
        i++;
    }
    fclose(fp);
}



void create_comma_separated_file()
{
    int i = 0;
    timeval time_diff;
    FILE *fp;
    timeval start_time = data_buffer[0].timestamp;

    printf("Creating log.csv\n");
    fp = fopen("log.csv","w");

    fprintf(fp, "idx,raw,timestamp,BSY,SEL,C/D,I/O,MSG,,REQ,ACK,ATN,RST,Data\n");

    while(i < data_idx)
    {
        timersub(&(data_buffer[i].timestamp), &start_time, &time_diff);
        fprintf(fp, "%d,%08lX,%ld.%ld,",data_idx, data_buffer[i].data, time_diff.tv_sec, time_diff.tv_usec);
        fprintf(fp, "%d,", get_pin_value(data_buffer[i].data, PIN_BSY));
        fprintf(fp, "%d,", get_pin_value(data_buffer[i].data, PIN_SEL));
        fprintf(fp, "%d,", get_pin_value(data_buffer[i].data, PIN_CD));
        fprintf(fp, "%d,", get_pin_value(data_buffer[i].data, PIN_IO));
        fprintf(fp, "%d,", get_pin_value(data_buffer[i].data, PIN_MSG));
        fprintf(fp, "%d,", get_pin_value(data_buffer[i].data, PIN_REQ));
        fprintf(fp, "%d,", get_pin_value(data_buffer[i].data, PIN_ACK));
        fprintf(fp, "%d,", get_pin_value(data_buffer[i].data, PIN_ATN));
        fprintf(fp, "%d,", get_pin_value(data_buffer[i].data, PIN_RST));
        fprintf(fp, "%02X,", get_data_field(data_buffer[i].data));
        fprintf(fp, "\n");
        i++;
    }
    fclose(fp);
}



//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void Cleanup()
{

    printf("In cleanup....\n");
    create_comma_separated_file();
    create_value_change_dump();

	// Cleanup the Bus
	bus->Cleanup();

	// Discard the GPIOBUS object
	delete bus;

}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
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
	cpu_set_t cpuset;
	int cpus;

	// Get the number of CPUs
	CPU_ZERO(&cpuset);
	sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
	cpus = CPU_COUNT(&cpuset);

	// Set the thread affinity
	if (cpu < cpus) {
		CPU_ZERO(&cpuset);
		CPU_SET(cpu, &cpuset);
		sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
	}
}

static DWORD high_bits = 0x0;
static DWORD low_bits = 0xFFFFFFFF;


//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
#ifdef BAREMETAL
extern "C"
int startrascsi(void)
{
	int argc = 0;
	char** argv = NULL;
#else
int main(int argc, char* argv[])
{
#endif	// BAREMETAL
    DWORD prev_high = high_bits;
    DWORD prev_low = low_bits;
    DWORD prev_sample = 0xFFFFFFFF;
    DWORD this_sample = 0;
	int ret;
    struct sched_param schparam;

    spdlog::set_level(spdlog::level::trace);
    spdlog::trace("Entering the function with {0:x}{1:X} arguments", argc,20);
	// Output the Banner
	Banner(argc, argv);
    memset(data_buffer,0,sizeof(data_buffer));

	// Initialize
	ret = 0;
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
	running = TRUE;
	bus->SetACT(FALSE);

    spdlog::trace("Going into running mode {}", 1);
    printf("ALL_SCSI_PINS %08X\n",ALL_SCSI_PINS);
	// Main Loop
	while (running) {
		// Work initialization
		this_sample = (bus->Aquire() & ALL_SCSI_PINS);

		if(this_sample != prev_sample)
		{

            // This is intended to be a debug check to see if every pin is set
            // high and low at some point.
            high_bits |= this_sample;
            low_bits &= this_sample;

            if ((high_bits != prev_high) || (low_bits != prev_low))
            {
                printf("   %08lX    %08lX\n",high_bits, low_bits);
            }
            prev_high = high_bits;
            prev_low = low_bits;

            //printf("%d Sample %08lX\n", data_idx, this_sample);
            data_buffer[data_idx].data = this_sample;
            (void)gettimeofday(&(data_buffer[data_idx].timestamp), NULL);
            data_idx = (data_idx + 1) % MAX_BUFF_SIZE;
            prev_sample = this_sample;
		}

		continue;
	}

	// Cleanup
	Cleanup();

init_exit:
	exit(ret);
}
