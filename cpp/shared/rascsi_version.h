//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2020 akuker
//	[ Define the version string ]
//
//---------------------------------------------------------------------------
#pragma once

#include <string>

extern const int rascsi_major_version; // Last two digits of year
extern const int rascsi_minor_version; // Month
extern const int rascsi_patch_version; // Patch number

std::string rascsi_get_version_string();
