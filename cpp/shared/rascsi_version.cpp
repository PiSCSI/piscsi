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
#include <sstream>
#include <iomanip>

// The following should be updated for each release
const int rascsi_major_version = 22; // Last two digits of year
const int rascsi_minor_version = 12; // Month
const int rascsi_patch_version = -1;  // Patch number - increment for each update

using namespace std;

string rascsi_get_version_string()
{
	stringstream s;

	s << setw(2) << setfill('0') << rascsi_major_version << '.' << rascsi_minor_version;

	if (rascsi_patch_version < 0) {
		s << " --DEVELOPMENT BUILD--";
    }
    else {
		s << '.' << rascsi_patch_version;
    }

	return s.str();
}


