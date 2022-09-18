//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2020 akuker
//	[ Define the version string ]
//
//---------------------------------------------------------------------------

#include "rascsi_version.h"
#include <iomanip>

// The following should be updated for each release
const int rascsi_major_version = 22; // Last two digits of year
const int rascsi_minor_version = 9; // Month
const int rascsi_patch_version = -1;  // Patch number - increment for each update

using namespace std;

string rascsi_get_version_string()
{
	ostringstream os;

	os << setw(2) << setfill('0') << rascsi_major_version << '.' << setw(2) << setfill('0') << rascsi_minor_version;

	if (rascsi_patch_version < 0) {
		os << " --DEVELOPMENT BUILD--";
    }
    else {
		os << '.' << rascsi_patch_version;
    }

	return os.str();
}


