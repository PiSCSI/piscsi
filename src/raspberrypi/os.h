//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2018 GIMONS
//
//	Imported NetBSD support and some optimisation patch by Rin Okuyama.
//
//	[ OS固有 ]
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <utime.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <sched.h>
#include <poll.h>
#include <pthread.h>
#include <dirent.h>
#include <iconv.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#if defined(__linux__)
#include <linux/if.h>
#include <linux/if_tun.h>
#elif defined(__NetBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_tap.h>
#include <ifaddrs.h>
#else
#error unsupported platform
#endif

//---------------------------------------------------------------------------
//
//	基本マクロ
//
//---------------------------------------------------------------------------
#undef FASTCALL
#define FASTCALL

#undef CDECL
#define CDECL

#undef INLINE
#define INLINE

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

//---------------------------------------------------------------------------
//
//	基本型定義
//
//---------------------------------------------------------------------------
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
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
#define _MAX_DRIVE  3
#define _MAX_DIR    256
#define _MAX_FNAME  256
#define _MAX_EXT    256

#define off64_t off_t

#define xstrcasecmp strcasecmp
#define xstrncasecmp strncasecmp

#endif	// os_h
