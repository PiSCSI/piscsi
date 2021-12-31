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
#include <getopt.h>
#include "rascsi.h"
#include "gpio_sim.h"
using namespace std;

// static BYTE prev_value[32] = {0xFF};
// static volatile bool running;		// Running flag
// GPIOBUS *bus;						// GPIO Bus

data_capture *data_capture_array;
DWORD max_samples = 1000000;
int actual_samples = 0;

const char* json_filename = "./radius_tiny.json";

const char timestamp_label[] = "\"timestamp\":\"0x";
const char data_label[] = "\"data\":\"0x";


void read_json()
{
    char str_buf[1024];
    FILE *fp = fopen(json_filename, "r");
    int idx=0;

    while(fgets(str_buf, sizeof(str_buf), fp))
    {

        char timestamp[1024];
        char data[1024];
        uint64_t timestamp_uint;
        uint32_t data_uint;
        char *ptr;

        char *timestamp_str;
        char *data_str;
        timestamp_str = strnstr(str_buf, timestamp_label, sizeof(str_buf));
        if(!timestamp_str)continue;
        strncpy(timestamp, &timestamp_str[strlen(timestamp_label)],16);
        timestamp[16] = '\0';
        timestamp_uint = strtoull(timestamp,&ptr,16);

        data_str = strnstr(str_buf, data_label, sizeof(str_buf));
        if(!data_str)continue;
        strncpy(data, &data_str[strlen(data_label)],8);
        data[8] = '\0';
        data_uint = strtoul(data, &ptr, 16);

        // printf("Time: %016llX Data: %08X\n", timestamp_uint, data_uint);

        data_capture_array[idx].timestamp = timestamp_uint;
        data_capture_array[idx].data = data_uint;
        idx++;
    }
    fclose(fp);
    actual_samples = idx;
}

void dump_data(){
    data_capture *data;

    for(int idx=0; idx<actual_samples; idx++){
        data = &data_capture_array[idx];
        printf(" BSY:%d MSG:%d IO:%d CD:%d DATA:%02X\n",GetBsy(data),GetMsg(data),GetIo(data),GetCd(data),GetData(data));
    }
}

void decode_data(){
    data_capture *data;
    bool prev_data_valid = false;
    bool curr_data_valid;
    DWORD selected_id = 0;

    for(int idx=0; idx<actual_samples; idx++){
        data = &data_capture_array[idx];
        curr_data_valid = GetAck(data) && GetReq(data);
        BUS::phase_t phase = GetPhase(data);
        if(phase == BUS::selection && !GetBsy(data)){
            selected_id = GetData(data);
        }
        if(curr_data_valid && !prev_data_valid){
            printf("[%02X] Time: %lf Data Sample: %02X Phase: %s\n", selected_id, ((double)data->timestamp/10)/1000, GetData(data), GetPhaseStr(data));
        }
        prev_data_valid = curr_data_valid;

        //     prev_data_valid
        // printf(" BSY:%d MSG:%d IO:%d CD:%d DATA:%02X\n",GetBsy(data),GetMsg(data),GetIo(data),GetCd(data),GetData(data));
    }


}

//---------------------------------------------------------------------------
//
//	Main processing
//
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    printf("starting\n");
    data_capture_array = (data_capture*)malloc(max_samples * sizeof(data_capture));
    printf("reading json\n");
    read_json();
    printf("Actual samples: %d\n", actual_samples);
    // dump_data();
    decode_data();
}