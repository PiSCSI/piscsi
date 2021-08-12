//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2020 akuker
//	[ Define the version string]
//
//---------------------------------------------------------------------------
#pragma once

extern const int rascsi_major_version; // Last two digits of year
extern const int rascsi_minor_version; // Month
extern const int rascsi_patch_version; // Patch number

//---------------------------------------------------------------------------
//
//	Fetch the version string
//
//---------------------------------------------------------------------------
const char* rascsi_get_version_string();
