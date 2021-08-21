//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2020 akuker
//
//	Imported NetBSD support and some optimisation patch by Rin Okuyama.
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

#if defined(__linux__)
#include <linux/gpio.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#elif defined(__NetBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_tap.h>
#include <ifaddrs.h>
#endif

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

#if !defined(ASSERT_DIAG)
#if !defined(NDEBUG)
#define ASSERT_DIAG()	AssertDiag()
#else
#define ASSERT_DIAG()	((void)0)
#endif	// NDEBUG
#endif	// ASSERT_DIAG

#define ARRAY_SIZE(x) (sizeof(x)/(sizeof(x[0])))

//---------------------------------------------------------------------------
//
//	Basic Type Definitions
//
//---------------------------------------------------------------------------
typedef unsigned char BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;
typedef int BOOL;
typedef char TCHAR;
typedef char *LPTSTR;
typedef const char *LPCTSTR;
typedef const char *LPCSTR;

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
