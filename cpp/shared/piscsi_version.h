//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Copyright (C) 2020 akuker
//	[ Define the version string ]
//
//---------------------------------------------------------------------------
#pragma once

#include <string>

extern const int piscsi_major_version; // Last two digits of year
extern const int piscsi_minor_version; // Month
extern const int piscsi_patch_version; // Patch number

std::string piscsi_get_version_string();
