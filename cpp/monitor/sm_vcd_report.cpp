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

#include "hal/data_sample.h"
#include "hal/gpiobus.h"
#include "shared/log.h"
#include "sm_core.h"
#include "sm_reports.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

//---------------------------------------------------------------------------
//
//  Constant declarations
//
//---------------------------------------------------------------------------

// Symbol definition for the VCD file
// These are just arbitrary symbols. They can be anything allowed by the VCD file format,
// as long as they're consistently used.
const char SYMBOL_PIN_DAT   = '#';
const char SYMBOL_PIN_ATN   = '+';
const char SYMBOL_PIN_RST   = '$';
const char SYMBOL_PIN_ACK   = '%';
const char SYMBOL_PIN_REQ   = '^';
const char SYMBOL_PIN_MSG   = '&';
const char SYMBOL_PIN_CD    = '*';
const char SYMBOL_PIN_IO    = '(';
const char SYMBOL_PIN_BSY   = ')';
const char SYMBOL_PIN_SEL   = '-';
const char SYMBOL_PIN_PHASE = '=';

// We'll use position 0 in the prev_value array to store the previous phase
const int PIN_PHASE = 0;

//---------------------------------------------------------------------------
//
//	Variable declarations
//
//---------------------------------------------------------------------------
static uint8_t prev_value[32] = {0xFF};

// static uint8_t get_pin_value(DataSample *data, int pin)
// {
//     return data->GetSignal(pin);
// }

// static uint8_t get_data_field(DataSample *data)
// {
//     return data->GetDAT();
// }

static void vcd_output_if_changed_phase(ofstream &fp, phase_t data, int pin, char symbol)
{
    if (prev_value[pin] != static_cast<uint8_t>(data)) {
        prev_value[pin] = static_cast<uint8_t>(data);
        fp << "s" << BUS::GetPhaseStrRaw(data) << " " << symbol << endl;
    }
}

static void vcd_output_if_changed_bool(ofstream &fp, bool data, int pin, char symbol)
{
    if (prev_value[pin] != data) {
        prev_value[pin] = data;
        fp << data << symbol << endl;
    }
}

static void vcd_output_if_changed_byte(ofstream &fp, uint8_t data, int pin, char symbol)
{
    if (prev_value[pin] != data) {
        prev_value[pin] = data;
        fp << "b" << fmt::format("{0:8b}", data) << " " << symbol << endl;
    }
}

void scsimon_generate_value_change_dump(string filename, const vector<shared_ptr<DataSample>> &data_capture_array)
{
    LOGTRACE("Creating Value Change Dump file (%s)", filename.c_str())
    ofstream vcd_ofstream;
    vcd_ofstream.open(filename.c_str(), ios::out);

    // Get the current time
    time_t rawtime;
    time(&rawtime);
    const struct tm *timeinfo = localtime(&rawtime);
    char timestamp[256];
    strftime(timestamp, sizeof(timestamp), "%d-%m-%Y %H-%M-%S", timeinfo);

    vcd_ofstream << "$date" << endl
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
                 << "$var wire 1 " << SYMBOL_PIN_CD << " CD $end" << endl
                 << "$var wire 1 " << SYMBOL_PIN_IO << " IO $end" << endl
                 << "$var wire 1 " << SYMBOL_PIN_MSG << " MSG $end" << endl
                 << "$var wire 1 " << SYMBOL_PIN_REQ << " REQ $end" << endl
                 << "$var wire 1 " << SYMBOL_PIN_ACK << " ACK $end" << endl
                 << "$var wire 1 " << SYMBOL_PIN_ATN << " ATN $end" << endl
                 << "$var wire 1 " << SYMBOL_PIN_RST << " RST $end" << endl
                 << "$var wire 8 " << SYMBOL_PIN_DAT << " data $end" << endl
                 << "$var string 1 " << SYMBOL_PIN_PHASE << " phase $end" << endl
                 << "$upscope $end" << endl
                 << "$enddefinitions $end" << endl;

    // Initial values - default to zeros
    vcd_ofstream << "$dumpvars" << endl
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
    for (shared_ptr<DataSample> cur_sample : data_capture_array) {
        vcd_ofstream << "#" << cur_sample->GetTimestamp() * ScsiMon::ns_per_loop << endl;
        vcd_output_if_changed_bool(vcd_ofstream, cur_sample->GetBSY(), PIN_BSY, SYMBOL_PIN_BSY);
        vcd_output_if_changed_bool(vcd_ofstream, cur_sample->GetSEL(), PIN_SEL, SYMBOL_PIN_SEL);
        vcd_output_if_changed_bool(vcd_ofstream, cur_sample->GetCD(), PIN_CD, SYMBOL_PIN_CD);
        vcd_output_if_changed_bool(vcd_ofstream, cur_sample->GetIO(), PIN_IO, SYMBOL_PIN_IO);
        vcd_output_if_changed_bool(vcd_ofstream, cur_sample->GetMSG(), PIN_MSG, SYMBOL_PIN_MSG);
        vcd_output_if_changed_bool(vcd_ofstream, cur_sample->GetREQ(), PIN_REQ, SYMBOL_PIN_REQ);
        vcd_output_if_changed_bool(vcd_ofstream, cur_sample->GetACK(), PIN_ACK, SYMBOL_PIN_ACK);
        vcd_output_if_changed_bool(vcd_ofstream, cur_sample->GetATN(), PIN_ATN, SYMBOL_PIN_ATN);
        vcd_output_if_changed_bool(vcd_ofstream, cur_sample->GetRST(), PIN_RST, SYMBOL_PIN_RST);
        vcd_output_if_changed_byte(vcd_ofstream, cur_sample->GetDAT(), PIN_DT0, SYMBOL_PIN_DAT);
        vcd_output_if_changed_phase(vcd_ofstream, cur_sample->GetPhase(), PIN_PHASE, SYMBOL_PIN_PHASE);
        i++;
    }
    vcd_ofstream.close();
}
