
//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "linux_os_stubs.h"
#include <boost/filesystem.hpp>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

using namespace std;

const boost::filesystem::path test_path("/scsi-test");

void test_fs_create_file_in_temp_dir(string filename, vector<uint8_t> &data)
{
    boost::filesystem::path new_filename = boost::filesystem::temp_directory_path();
    new_filename += test_path;
    new_filename += boost::filesystem::path(filename);

    boost::filesystem::create_directories(new_filename.parent_path());

    FILE *fp = fopen(new_filename.c_str(), "wb");
    if (fp == nullptr) {
        printf("ERROR: Unable to open file %s", new_filename.c_str());
        return;
    }

    size_t size_written = fwrite(&data[0], sizeof(uint8_t), data.size(), fp);

    if (size_written != sizeof(vector<uint8_t>::value_type) * data.size()) {
        printf("Expected to write %lu bytes, but only wrote %lu to %s", size_written,
               sizeof(vector<uint8_t>::value_type) * data.size(), filename.c_str());
    }
    fclose(fp);
}

void test_fs_delete_file_in_temp_dir(string filename)
{
    boost::filesystem::path temp_file = boost::filesystem::temp_directory_path();
    temp_file += test_path;
    temp_file += boost::filesystem::path(filename);
    boost::filesystem::remove(temp_file);
}

void test_fs_cleanup_temp_files()
{
    boost::filesystem::path temp_path = boost::filesystem::temp_directory_path();
    temp_path += test_path;
    boost::filesystem::remove_all(temp_path);
}

extern "C" {

#ifdef __USE_LARGEFILE64
FILE *__wrap_fopen64(const char *__restrict __filename, const char *__restrict __modes)
#else
FILE *__wrap_fopen(const char *__restrict __filename, const char *__restrict __modes)
#endif
{
    boost::filesystem::path new_filename;
    bool create_directory = false;

    // If we're trying to open up the device tree soc ranges,
    // re-direct it to a temporary local file.
    if (string(__filename) == "/proc/device-tree/soc/ranges") {
        create_directory = true;
        new_filename     = boost::filesystem::temp_directory_path();
        new_filename += test_path;
        new_filename += boost::filesystem::path(__filename);
    } else {
        new_filename = boost::filesystem::path(__filename);
    }

    if (create_directory) {
        boost::filesystem::create_directories(new_filename.parent_path());
    }
#ifdef __USE_LARGEFILE64
    return __real_fopen64(new_filename.c_str(), __modes);
#else
    return __real_fopen(new_filename.c_str(), __modes);
#endif
}

} // end extern "C"