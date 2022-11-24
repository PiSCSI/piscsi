//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#include "hal/data_sample_raspberry.h"
#include "shared/log.h"
#include "sm_reports.h"
#include "string.h"
#include <fstream>
#include <iostream>

using namespace std;

const char timestamp_label[] = "\"timestamp\":\"0x";
const char data_label[]      = "\"data\":\"0x";

uint32_t scsimon_read_json(const char *json_filename, vector<shared_ptr<DataSample>> &data_capture_array)
{
    char str_buf[1024];
    FILE *fp              = fopen(json_filename, "r");
    uint32_t sample_count = 0;

    while (fgets(str_buf, sizeof(str_buf), fp)) {
        char timestamp[1024];
        char data[1024];
        uint64_t timestamp_uint;
        uint32_t data_uint;
        char *ptr;

        const char *timestamp_str = strstr(str_buf, timestamp_label);
        if (!timestamp_str)
            continue;
        strncpy(timestamp, &timestamp_str[strlen(timestamp_label)], 16);
        timestamp[16]  = '\0';
        timestamp_uint = strtoull(timestamp, &ptr, 16);

        const char *data_str = strstr(str_buf, data_label);
        if (!data_str)
            continue;
        strncpy(data, &data_str[strlen(data_label)], 8);
        data[8]   = '\0';
        data_uint = static_cast<uint32_t>(strtoul(data, &ptr, 16));

        // For reading in JSON files, we'll just assume raspberry pi data types
        // shared_ptr<DataSample> read_sample = ;
        data_capture_array.push_back(make_unique<DataSample_Raspberry>(data_uint, timestamp_uint));

        sample_count++;
        if (sample_count == UINT32_MAX) {
            LOGWARN("Maximum number of samples read. Some data may not be included.")
            break;
        }
    }

    if (fp != nullptr) {
        fclose(fp);
    }

    return sample_count;
}

//---------------------------------------------------------------------------
//
//	Generate JSON Output File
//
//---------------------------------------------------------------------------
void scsimon_generate_json(const char *filename, vector<shared_ptr<DataSample>> data_capture_array)
{
    LOGTRACE("Creating JSON file (%s)", filename)
    ofstream json_ofstream;
    json_ofstream.open(filename, ios::out);

    json_ofstream << "[" << endl;

    uint32_t i = 0;
    uint32_t capture_count = data_capture_array.size();
    for(auto data: data_capture_array){
        json_ofstream << fmt::format("{{\"id\": \"{0:d}\", \"timestamp\":\"{1:#016x}\", \"data\":\"{2:#08x}\"}}", i,
                                     data->GetTimestamp(), data->GetRawCapture());

        if (i != (capture_count - 1)) {
            json_ofstream << ",";
        }
        json_ofstream << endl;
        i++;
    }
    json_ofstream << "]" << endl;
    json_ofstream.close();
}
