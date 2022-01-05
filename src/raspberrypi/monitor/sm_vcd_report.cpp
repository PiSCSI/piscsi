//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*) for Raspberry Pi
//
//	Copyright (C) 2020-2021 akuker
//
//	[ SCSI Bus Monitor ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "log.h"
#include "spdlog/spdlog.h"
#include <sstream>
#include <iostream>
#include "data_sample.h"
#include "sm_reports.h"

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

// We'll use position 0 in the prev_value array to store the previous phase
#define PIN_PHASE 0

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
static BYTE prev_value[32] = {0xFF};

extern double ns_per_loop;

static BOOL get_pin_value(DWORD data, int pin)
{
    return (data >> pin) & 1;
}

static BYTE get_data_field(DWORD data)
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

static void vcd_output_if_changed_phase(FILE *fp, DWORD data, int pin, char symbol)
{
    BUS::phase_t new_value = GPIOBUS::GetPhaseRaw(data);
    if (prev_value[pin] != new_value)
    {
        prev_value[pin] = new_value;
        fprintf(fp, "s%s %c\n", GPIOBUS::GetPhaseStrRaw(new_value), symbol);
    }
}

static void vcd_output_if_changed_bool(FILE *fp, DWORD data, int pin, char symbol)
{
    BOOL new_value = get_pin_value(data, pin);
    if (prev_value[pin] != new_value)
    {
        prev_value[pin] = new_value;
        fprintf(fp, "%d%c\n", new_value, symbol);
    }
}

static void vcd_output_if_changed_byte(FILE *fp, DWORD data, int pin, char symbol)
{
    BYTE new_value = get_data_field(data);
    if (prev_value[pin] != new_value)
    {
        prev_value[pin] = new_value;
        fprintf(fp, "b%d%d%d%d%d%d%d%d %c\n",
                get_pin_value(data, PIN_DT7),
                get_pin_value(data, PIN_DT6),
                get_pin_value(data, PIN_DT5),
                get_pin_value(data, PIN_DT4),
                get_pin_value(data, PIN_DT3),
                get_pin_value(data, PIN_DT2),
                get_pin_value(data, PIN_DT1),
                get_pin_value(data, PIN_DT0), symbol);
    }
}

void scsimon_generate_value_change_dump(const char *filename, const data_capture *data_capture_array, int capture_count)
{
    LOGTRACE("Creating Value Change Dump file (%s)", filename);
    FILE *fp = fopen(filename, "w");

    // Get the current time
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    char timestamp[256];
    strftime(timestamp, sizeof(timestamp), "%d-%m-%Y %H-%M-%S", timeinfo);

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
    fprintf(fp, "$var wire 1 %c CD $end\n", SYMBOL_PIN_CD);
    fprintf(fp, "$var wire 1 %c IO $end\n", SYMBOL_PIN_IO);
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
    fprintf(fp, "0%c\n", SYMBOL_PIN_CD);
    fprintf(fp, "0%c\n", SYMBOL_PIN_IO);
    fprintf(fp, "0%c\n", SYMBOL_PIN_MSG);
    fprintf(fp, "0%c\n", SYMBOL_PIN_REQ);
    fprintf(fp, "0%c\n", SYMBOL_PIN_ACK);
    fprintf(fp, "0%c\n", SYMBOL_PIN_ATN);
    fprintf(fp, "0%c\n", SYMBOL_PIN_RST);
    fprintf(fp, "b00000000 %c\n", SYMBOL_PIN_DAT);
    fprintf(fp, "$end\n");

    DWORD i = 0;
    while (i < capture_count)
    {
        ostringstream s;
        s << (uint64_t)(data_capture_array[i].timestamp * ns_per_loop);
        fprintf(fp, "#%s\n", s.str().c_str());
        vcd_output_if_changed_bool(fp, data_capture_array[i].data, PIN_BSY, SYMBOL_PIN_BSY);
        vcd_output_if_changed_bool(fp, data_capture_array[i].data, PIN_SEL, SYMBOL_PIN_SEL);
        vcd_output_if_changed_bool(fp, data_capture_array[i].data, PIN_CD, SYMBOL_PIN_CD);
        vcd_output_if_changed_bool(fp, data_capture_array[i].data, PIN_IO, SYMBOL_PIN_IO);
        vcd_output_if_changed_bool(fp, data_capture_array[i].data, PIN_MSG, SYMBOL_PIN_MSG);
        vcd_output_if_changed_bool(fp, data_capture_array[i].data, PIN_REQ, SYMBOL_PIN_REQ);
        vcd_output_if_changed_bool(fp, data_capture_array[i].data, PIN_ACK, SYMBOL_PIN_ACK);
        vcd_output_if_changed_bool(fp, data_capture_array[i].data, PIN_ATN, SYMBOL_PIN_ATN);
        vcd_output_if_changed_bool(fp, data_capture_array[i].data, PIN_RST, SYMBOL_PIN_RST);
        vcd_output_if_changed_byte(fp, data_capture_array[i].data, PIN_DT0, SYMBOL_PIN_DAT);
        vcd_output_if_changed_phase(fp, data_capture_array[i].data, PIN_PHASE, SYMBOL_PIN_PHASE);
        i++;
    }
    fclose(fp);
}
