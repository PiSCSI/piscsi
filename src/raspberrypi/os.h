//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2020 akuker
//
//	[ OS related definitions ]
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

//---------------------------------------------------------------------------
//
//	Basic Type Definitions
//
//---------------------------------------------------------------------------
using BYTE = unsigned char;
using WORD = uint16_t;
using DWORD = uint32_t;

#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE 1
#endif
