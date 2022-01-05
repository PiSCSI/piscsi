//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*) for Raspberry Pi
//
//	Copyright (C) 2020-2021 akuker
//
//	[ SCSI Bus Monitor ]
//
//---------------------------------------------------------------------------

#include "sm_reports.h"
#include "log.h"
#include "spdlog/spdlog.h"

using namespace std;

//---------------------------------------------------------------------------
//
//	Generate JSON Output File
//
//---------------------------------------------------------------------------
void scsimon_generate_json(const char* filename, const  data_capture *data_capture_array, int capture_count)
{
    LOGINFO("Creating JSON file (%s)", filename);
    FILE *fp = fopen(filename,"w");
    fprintf(fp, "[\n");

    DWORD i = 0;
    while (i < capture_count)
    {
        fprintf(fp, "   {\"id\":\"%d\", \"timestamp\":\"0x%016llX\", \"data\":\"0x%08X\" }",i, data_capture_array[i].timestamp, data_capture_array[i].data);
        if(i != (capture_count-1)){
            fprintf(fp, ",\n");
        }
        else{
            fprintf(fp, "\n");
        }
        i++;
    }
    fprintf(fp, "]\n");
    fclose(fp);
}
