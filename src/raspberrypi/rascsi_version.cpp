//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2020 akuker
//	[ Define the version string ]
//
//---------------------------------------------------------------------------

#include "rascsi_version.h"
#include <cstdio>

// The following should be updated for each release
const int rascsi_major_version = 21; // Last two digits of year
const int rascsi_minor_version = 12; // Month
const int rascsi_patch_version = -1;  // Patch number - increment for each update

static char rascsi_version_string[30]; // Allow for string up to "XX.XX.XXX" + null character + "development build"

//---------------------------------------------------------------------------
//
//	Get the RaSCSI version string
//
//---------------------------------------------------------------------------
const char* rascsi_get_version_string()
{
    if(rascsi_patch_version < 0)
    {
        snprintf(rascsi_version_string, sizeof(rascsi_version_string),
           "%02d.%02d --DEVELOPMENT BUILD--",
            rascsi_major_version,
            rascsi_minor_version);
    }
    else
    {
        snprintf(rascsi_version_string, sizeof(rascsi_version_string), "%02d.%02d.%d",
            rascsi_major_version,
            rascsi_minor_version, 
            rascsi_patch_version);
    }
    return rascsi_version_string;
}


