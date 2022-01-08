//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#include "sm_reports.h"
#include "log.h"
#include "spdlog/spdlog.h"
#include "string.h"
#include <iostream>
#include <fstream>
using namespace std;

const char timestamp_label[] = "\"timestamp\":\"0x";
const char data_label[] = "\"data\":\"0x";

DWORD scsimon_read_json(const char *json_filename, data_capture *data_capture_array, DWORD max_sz)
{
    char str_buf[1024];
    FILE *fp = fopen(json_filename, "r");
    DWORD sample_count = 0;

    while (fgets(str_buf, sizeof(str_buf), fp))
    {

        char timestamp[1024];
        char data[1024];
        uint64_t timestamp_uint;
        uint32_t data_uint;
        char *ptr;

        char *timestamp_str;
        char *data_str;
        timestamp_str = strstr(str_buf, timestamp_label);
        if (!timestamp_str)
            continue;
        strncpy(timestamp, &timestamp_str[strlen(timestamp_label)], 16);
        timestamp[16] = '\0';
        timestamp_uint = strtoull(timestamp, &ptr, 16);

        data_str = strstr(str_buf, data_label);
        if (!data_str)
            continue;
        strncpy(data, &data_str[strlen(data_label)], 8);
        data[8] = '\0';
        data_uint = strtoul(data, &ptr, 16);

        // printf("Time: %016llX Data: %08X\n", timestamp_uint, data_uint);

        data_capture_array[sample_count].timestamp = timestamp_uint;
        data_capture_array[sample_count].data = data_uint;
        sample_count++;
        if (sample_count >= max_sz)
        {
            LOGWARN("File exceeds maximum buffer size. Some data may be missing.");
            LOGWARN("Try re-running the tool with a larger buffer size");
            break;
        }
    }
    fclose(fp);

    return sample_count;
}

//---------------------------------------------------------------------------
//
//	Generate JSON Output File
//
//---------------------------------------------------------------------------
void scsimon_generate_json(const char *filename, const data_capture *data_capture_array, DWORD capture_count)
{
    LOGTRACE("Creating JSON file (%s)", filename);
    ofstream json_ofstream;
    json_ofstream.open(filename, ios::out);

    json_ofstream << "[" << endl;

    DWORD i = 0;
    while (i < capture_count)
    {
        json_ofstream << fmt::format("{{\"id\": \"{0:d}\", \"timestamp\":\"{1:#016x}\", \"data\":\"{2:#08x}\"}}", i, data_capture_array[i].timestamp, data_capture_array[i].data);

        if (i != (capture_count - 1))
        {
            json_ofstream << ",";
        }
        json_ofstream << endl;
        i++;
    }
    json_ofstream << "]" << endl;
    json_ofstream.close();

}
