//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

// This header file should ONLY be used in test procedures. It bypasses the
// standard c library functionality. DO NOT USE THIS IN PRODUCTION CODE.
#pragma once
#include <stdio.h>

extern "C" {
#ifdef __USE_LARGEFILE64
	FILE *__real_fopen64(const char *__restrict __filename, const char *__restrict __modes);
#else
	FILE *__real_fopen(const char *__restrict __filename, const char *__restrict __modes);
#endif
}
