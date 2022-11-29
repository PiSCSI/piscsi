//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

// This header file should ONLY be used in test procedures. It bypasses the
// linker wrap functionality. DO NOT USE THIS IN PRODUCTION CODE.
#pragma once
#include <boost/filesystem.hpp>
#include <stdio.h>
#include <string>
#include <vector>

using namespace std;

extern const boost::filesystem::path test_temp_dir;

// create a file with the specified data
void test_fs_create_file_in_temp_dir(string filename, vector<uint8_t> &data);

void test_fs_delete_file_in_temp_dir(string filename);
// Call this at the end of every test case to make sure things are cleaned up
void test_fs_cleanup_temp_files();

extern "C" {
#ifdef __USE_LARGEFILE64
FILE *__real_fopen64(const char *__restrict __filename, const char *__restrict __modes);
#else
FILE *__real_fopen(const char *__restrict __filename, const char *__restrict __modes);
#endif
}
