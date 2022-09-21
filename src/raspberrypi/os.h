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

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstdarg>
#include <cstring>
#include <csignal>
#include <cassert>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>
#include <sched.h>
#include <pthread.h>
#include <iconv.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <poll.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef __linux
#include <sys/epoll.h>
#include <linux/gpio.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

//---------------------------------------------------------------------------
//
//	Basic Type Definitions
//
//---------------------------------------------------------------------------
using BYTE = unsigned char;
using WORD = uint16_t;
using DWORD = uint32_t;
using TCHAR = char;

#if !defined(FALSE)
#define FALSE               0
#endif

#if !defined(TRUE)
#define TRUE                1
#endif

#if !defined(_T)
#define _T(x)	x
#endif

static const int _MAX_FNAME = 256;
