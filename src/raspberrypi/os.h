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

#if !defined(os_h)
#define os_h

//---------------------------------------------------------------------------
//
//	#define
//
//---------------------------------------------------------------------------
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#ifndef  _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

//---------------------------------------------------------------------------
//
//	#include
//
//---------------------------------------------------------------------------

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
#include <sys/epoll.h>
#include <netinet/in.h>
#include <linux/gpio.h>
#include <linux/if.h>
#include <linux/if_tun.h>

//---------------------------------------------------------------------------
//
//	Basic Macros
//
//---------------------------------------------------------------------------
#if !defined(ASSERT)
#if !defined(NDEBUG)
#define ASSERT(cond)	assert(cond)
#else
#define ASSERT(cond)	((void)0)
#endif	// NDEBUG
#endif	// ASSERT

#define ARRAY_SIZE(x) (sizeof(x)/(sizeof(x[0])))

//---------------------------------------------------------------------------
//
//	Basic Type Definitions
//
//---------------------------------------------------------------------------
typedef unsigned char BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int BOOL;
typedef char TCHAR;

#if !defined(FALSE)
#define FALSE               0
#endif

#if !defined(TRUE)
#define TRUE                1
#endif

#if !defined(_T)
#define _T(x)	x
#endif

#define _MAX_PATH   260
#define _MAX_DIR    256
#define _MAX_FNAME  256
#define _MAX_EXT    256

#endif	// os_h
