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
#include <fstream>
#include "data_sample.h"
#include "sm_reports.h"
#include "gpiobus.h"

using namespace std;

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------

// Symbol definition for the VCD file
// These are just arbitrary symbols. They can be anything allowed by the VCD file format,
// as long as they're consistently used.
const char SYMBOL_PIN_DAT  = '#';
const char SYMBOL_PIN_ATN  = '+';
const char SYMBOL_PIN_RST  = '$';
const char SYMBOL_PIN_ACK  = '%';
const char SYMBOL_PIN_REQ  = '^';
const char SYMBOL_PIN_MSG  = '&';
const char SYMBOL_PIN_CD   = '*';
const char SYMBOL_PIN_IO   = '(';
const char SYMBOL_PIN_BSY  = ')';
const char SYMBOL_PIN_SEL  = '-';
const char SYMBOL_PIN_PHASE  = '=';

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

static void vcd_output_if_changed_phase(ofstream& fp, DWORD data, int pin, char symbol)
{
    BUS::phase_t new_value = GPIOBUS::GetPhaseRaw(data);
    if (prev_value[pin] != new_value)
    {
        prev_value[pin] = new_value;
        fp << "s" << GPIOBUS::GetPhaseStrRaw(new_value) << " " << symbol << endl;
    }
}

static void vcd_output_if_changed_bool(ofstream& fp, DWORD data, int pin, char symbol)
{
    BOOL new_value = get_pin_value(data, pin);
    if (prev_value[pin] != new_value)
    {
        prev_value[pin] = new_value;
        fp << new_value << symbol << endl;
    }
}

static void vcd_output_if_changed_byte(ofstream& fp, DWORD data, int pin, char symbol)
{
    BYTE new_value = get_data_field(data);
    if (prev_value[pin] != new_value)
    {
        prev_value[pin] = new_value;
        fp << "b"
            << fmt::format("{0:b}", get_pin_value(data, PIN_DT7))
            << fmt::format("{0:b}", get_pin_value(data, PIN_DT6))
            << fmt::format("{0:b}", get_pin_value(data, PIN_DT5))
            << fmt::format("{0:b}", get_pin_value(data, PIN_DT4))
            << fmt::format("{0:b}", get_pin_value(data, PIN_DT3))
            << fmt::format("{0:b}", get_pin_value(data, PIN_DT2))
            << fmt::format("{0:b}", get_pin_value(data, PIN_DT1))
            << fmt::format("{0:b}", get_pin_value(data, PIN_DT0))
            << " " << symbol << endl;
    }
}

void scsimon_generate_value_change_dump(const char *filename, const data_capture *data_capture_array, DWORD capture_count)
{
    LOGTRACE("Creating Value Change Dump file (%s)", filename);
    ofstream vcd_ofstream;
    vcd_ofstream.open(filename, ios::out);

    // Get the current time
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    char timestamp[256];
    strftime(timestamp, sizeof(timestamp), "%d-%m-%Y %H-%M-%S", timeinfo);

    vcd_ofstream 
        << "$date" << endl
        << timestamp << endl
        << "$end" << endl
        << "$version" << endl
        << "   VCD generator tool version info text." << endl
        << "$end" << endl
        << "$comment" << endl
        << "   Tool build date:" << __TIMESTAMP__ << endl
        << "$end" << endl
        << "$timescale 1 ns $end" << endl
        << "$scope module logic $end" << endl
        << "$var wire 1 " << SYMBOL_PIN_BSY << " BSY $end" << endl
        << "$var wire 1 " << SYMBOL_PIN_SEL << " SEL $end" << endl
        << "$var wire 1 " << SYMBOL_PIN_CD  << " CD $end" << endl
        << "$var wire 1 " << SYMBOL_PIN_IO  << " IO $end"<< endl
        << "$var wire 1 " << SYMBOL_PIN_MSG << " MSG $end"<< endl
        << "$var wire 1 " << SYMBOL_PIN_REQ << " REQ $end" << endl
        << "$var wire 1 " << SYMBOL_PIN_ACK << " ACK $end" << endl
        << "$var wire 1 " << SYMBOL_PIN_ATN << " ATN $end" << endl
        << "$var wire 1 " << SYMBOL_PIN_RST << " RST $end" << endl
        << "$var wire 8 " << SYMBOL_PIN_DAT << " data $end" << endl
        << "$var string 1 " << SYMBOL_PIN_PHASE  << " phase $end" << endl
        << "$upscope $end" << endl
        << "$enddefinitions $end" << endl;

    // Initial values - default to zeros
    vcd_ofstream
        << "$dumpvars" << endl
        << "0" << SYMBOL_PIN_BSY << endl
        << "0" << SYMBOL_PIN_SEL << endl
        << "0" << SYMBOL_PIN_CD << endl
        << "0" << SYMBOL_PIN_IO << endl
        << "0" << SYMBOL_PIN_MSG << endl
        << "0" << SYMBOL_PIN_REQ << endl
        << "0" << SYMBOL_PIN_ACK << endl
        << "0" << SYMBOL_PIN_ATN << endl
        << "0" << SYMBOL_PIN_RST << endl
        << "b00000000 " << SYMBOL_PIN_DAT << endl
        << "$end" << endl;

    DWORD i = 0;
    while (i < capture_count)
    {
        vcd_ofstream << "#" << (uint64_t)(data_capture_array[i].timestamp * ns_per_loop) << endl;
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data, PIN_BSY, SYMBOL_PIN_BSY);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data, PIN_SEL, SYMBOL_PIN_SEL);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data, PIN_CD, SYMBOL_PIN_CD);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data, PIN_IO, SYMBOL_PIN_IO);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data, PIN_MSG, SYMBOL_PIN_MSG);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data, PIN_REQ, SYMBOL_PIN_REQ);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data, PIN_ACK, SYMBOL_PIN_ACK);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data, PIN_ATN, SYMBOL_PIN_ATN);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data, PIN_RST, SYMBOL_PIN_RST);
        vcd_output_if_changed_byte(vcd_ofstream, data_capture_array[i].data, PIN_DT0, SYMBOL_PIN_DAT);
        vcd_output_if_changed_phase(vcd_ofstream, data_capture_array[i].data, PIN_PHASE, SYMBOL_PIN_PHASE);
        i++;
    }
    vcd_ofstream.close();
}
