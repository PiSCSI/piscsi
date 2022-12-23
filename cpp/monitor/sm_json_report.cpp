//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2021-2022 akuker
//
//---------------------------------------------------------------------------

#include "hal/data_sample_raspberry.h"
#include "shared/log.h"
#include "sm_reports.h"
#include "string.h"
#include <fstream>
#include <iostream>

using namespace std;

const string timestamp_label = "\"timestamp\":\"0x";
const string data_label      = "\"data\":\"0x";

uint32_t scsimon_read_json(const string &json_filename, vector<shared_ptr<DataSample>> &data_capture_array)
{
    std::ifstream json_file(json_filename);
    uint32_t sample_count = 0;

    while (json_file) {
        string str_buf;
        std::getline(json_file, str_buf);
        string timestamp;
        string data;
        uint64_t timestamp_uint;
        uint32_t data_uint;
        char *ptr;

        size_t timestamp_pos = str_buf.find(timestamp_label);
        if (timestamp_pos == string::npos)
            continue;
        timestamp      = str_buf.substr(timestamp_pos + timestamp_label.length(), 16);
        timestamp_uint = strtoull(timestamp.c_str(), &ptr, 16);

        size_t data_pos = str_buf.find(data_label);
        if (data_pos == string::npos)
            continue;
        data      = str_buf.substr(data_pos + data_label.length(), 8);
        data_uint = static_cast<uint32_t>(strtoul(data.c_str(), &ptr, 16));

        // For reading in JSON files, we'll just assume raspberry pi data types
        data_capture_array.push_back(make_unique<DataSample_Raspberry>(data_uint, timestamp_uint));

        sample_count++;
        if (sample_count == UINT32_MAX) {
            LOGWARN("Maximum number of samples read. Some data may not be included.")
            break;
        }
    }

    json_file.close();

    return sample_count;
}

//---------------------------------------------------------------------------
//
//	Generate JSON Output File
//
//---------------------------------------------------------------------------
void scsimon_generate_json(const string &filename, const vector<shared_ptr<DataSample>> &data_capture_array)
{
    LOGTRACE("Creating JSON file (%s)", filename.c_str())
    ofstream json_ofstream;
    json_ofstream.open(filename.c_str(), ios::out);

    json_ofstream << "[" << endl;

    size_t i             = 0;
    size_t capture_count = data_capture_array.size();
    for (auto data : data_capture_array) {
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
