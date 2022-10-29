//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2020-2021 akuker
//
//	[ SCSI Bus Monitor ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "log.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include "data_sample.h"
#include "sm_reports.h"
#include "hal/gpiobus.h"

using namespace std;

extern shared_ptr<GPIOBUS> bus;		

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
const char SYMBOL_PIN_PHASE = '=';

// We'll use position 0 in the prev_value array to store the previous phase
const int PIN_PHASE = 0;

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
// TODO: prev_value can be smaller. Just making up a big number for now
static BYTE prev_value[128] = {0xFF};

extern double ns_per_loop;

static BYTE get_pin_value(uint32_t data, board_type::pi_physical_pin_e pin)
{
    return bus->GetPinRaw(data, pin);
    // return (data >> pin) & 1;
}

static BYTE get_data_field(uint32_t data)
{
    // TODO: This is a quick hack to re-use the GetData() function from data_sample.h
    const struct data_capture dummy_data_capture =
    {
        .data = data,
        .timestamp = 0,
    };
    return GetData(&dummy_data_capture);

}

static void vcd_output_if_changed_phase(ofstream& fp, uint32_t data, int pin, char symbol)
{
    const BUS::phase_t new_value = GPIOBUS::GetPhaseRaw(data);
    if (prev_value[pin] != (int)new_value) {
        prev_value[pin] = (int)new_value;
        fp << "s" << GPIOBUS::GetPhaseStrRaw(new_value) << " " << symbol << endl;
    }
}

static void vcd_output_if_changed_bool(ofstream& fp, uint32_t data, board_type::pi_physical_pin_e pin, char symbol)
{
    const BYTE new_value = get_pin_value(data, pin);
    if (prev_value[(int)pin] != new_value) {
        prev_value[(int)pin] = new_value;
        fp << new_value << symbol << endl;
    }
}

static void vcd_output_if_changed_byte(ofstream& fp, uint32_t data, int pin, char symbol)
{
	const BYTE new_value = get_data_field(data);
    if (prev_value[pin] != new_value) {
        prev_value[pin] = new_value;
        fp << "b"
            << fmt::format("{0:b}", get_pin_value(data, bus->GetBoard()->pin_dt7))
            << fmt::format("{0:b}", get_pin_value(data, bus->GetBoard()->pin_dt6))
            << fmt::format("{0:b}", get_pin_value(data, bus->GetBoard()->pin_dt5))
            << fmt::format("{0:b}", get_pin_value(data, bus->GetBoard()->pin_dt4))
            << fmt::format("{0:b}", get_pin_value(data, bus->GetBoard()->pin_dt3))
            << fmt::format("{0:b}", get_pin_value(data, bus->GetBoard()->pin_dt2))
            << fmt::format("{0:b}", get_pin_value(data, bus->GetBoard()->pin_dt1))
            << fmt::format("{0:b}", get_pin_value(data, bus->GetBoard()->pin_dt0))
            << " " << symbol << endl;
    }
}

void scsimon_generate_value_change_dump(const char *filename, const data_capture *data_capture_array, uint32_t capture_count)
{
    LOGTRACE("Creating Value Change Dump file (%s)", filename)
    ofstream vcd_ofstream;
    vcd_ofstream.open(filename, ios::out);

    // Get the current time
    time_t rawtime;
    time(&rawtime);
    const struct tm *timeinfo = localtime(&rawtime);
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

    uint32_t i = 0;
    while (i < capture_count) {
        vcd_ofstream << "#" << (uint64_t)(data_capture_array[i].timestamp * ns_per_loop) << endl;
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data,  bus->GetBoard()->pin_bsy, SYMBOL_PIN_BSY);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data,  bus->GetBoard()->pin_sel, SYMBOL_PIN_SEL);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data,  bus->GetBoard()->pin_cd, SYMBOL_PIN_CD);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data,  bus->GetBoard()->pin_io, SYMBOL_PIN_IO);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data,  bus->GetBoard()->pin_msg, SYMBOL_PIN_MSG);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data,  bus->GetBoard()->pin_req, SYMBOL_PIN_REQ);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data,  bus->GetBoard()->pin_ack, SYMBOL_PIN_ACK);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data,  bus->GetBoard()->pin_atn, SYMBOL_PIN_ATN);
        vcd_output_if_changed_bool(vcd_ofstream, data_capture_array[i].data,  bus->GetBoard()->pin_rst, SYMBOL_PIN_RST);
        vcd_output_if_changed_byte(vcd_ofstream, data_capture_array[i].data,  (int)bus->GetBoard()->pin_dt0, SYMBOL_PIN_DAT);
        vcd_output_if_changed_phase(vcd_ofstream, data_capture_array[i].data, PIN_PHASE, SYMBOL_PIN_PHASE);
        i++;
    }
    vcd_ofstream.close();
}
