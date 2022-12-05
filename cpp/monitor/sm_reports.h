//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Copyright (C) 2020-2021 akuker
//
//	[ SCSI Bus Monitor ]
//
//---------------------------------------------------------------------------

#include "hal/data_sample.h"
#include <memory>

uint32_t scsimon_read_json(const string &json_filename, vector<shared_ptr<DataSample>> &data_capture_array);

void scsimon_generate_html(const string &filename, const vector<shared_ptr<DataSample>> &data_capture_array);
void scsimon_generate_json(const string &filename, const vector<shared_ptr<DataSample>> &data_capture_array);
void scsimon_generate_value_change_dump(const string &filename, const vector<shared_ptr<DataSample>> &data_capture_array);
