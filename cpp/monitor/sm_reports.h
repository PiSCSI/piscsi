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

#include "data_sample.h"

uint32_t scsimon_read_json(const char *json_filename, data_capture *data_capture_array, uint32_t max_sz);

void scsimon_generate_html(const char *filename, const data_capture *data_capture_array, uint32_t capture_count);
void scsimon_generate_json(const char *filename, const data_capture *data_capture_array, uint32_t capture_count);
void scsimon_generate_value_change_dump(const char *filename, const data_capture *data_capture_array, uint32_t capture_count);
