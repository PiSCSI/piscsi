//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Copyright (C) 2020 akuker
//
//---------------------------------------------------------------------------

#include "piscsi_version.h"
#include <sstream>
#include <iomanip>

// The following should be updated for each release
const int piscsi_major_version = 24; // Last two digits of year
const int piscsi_minor_version =  4; // Month
const int piscsi_patch_version =  1; // Patch number - increment for each update

using namespace std;

string piscsi_get_version_string()
{
	stringstream s;

	s << setw(2) << setfill('0') << piscsi_major_version << '.' << setw(2) << piscsi_minor_version;

	if (piscsi_patch_version < 0) {
		s << " --DEVELOPMENT BUILD--";
    }
    else {
		s << '.' << setw(2) << piscsi_patch_version;
    }

	return s.str();
}


