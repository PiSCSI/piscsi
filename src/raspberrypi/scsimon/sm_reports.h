//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*) for Raspberry Pi
//
//	Copyright (C) 2020-2021 akuker
//
//	[ SCSI Bus Monitor ]
//
//---------------------------------------------------------------------------

#include "data_sample.h"

void scsimon_generate_html(const char* filename, const  data_capture *data_capture_array, int capture_count);
void scsimon_generate_json(const char* filename, const  data_capture *data_capture_array, int capture_count);
void scsimon_generate_value_change_dump(const char* filename, const  data_capture *data_capture_array, int capture_count);
