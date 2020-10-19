//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2020 akuker
//	[ Logging utilities ]
//
//---------------------------------------------------------------------------

#if !defined(log_h)
#define log_h

#include "spdlog/spdlog.h"
#include "spdlog/sinks/sink.h"

#define SPDLOGWRAPPER(loglevel, ...)\
 do{ char buf[256]; \
	 snprintf(buf, sizeof(buf),__VA_ARGS__);  \
	 spdlog::log(loglevel,buf);}while(0);

#ifndef DEBUG
// If we're doing a non-debug build, we want to skip the overhead of
// formatting the string, then calling the logger
#define LOGTRACE(...)     ((void)0)
#define LOGDEBUG(...)     ((void)0)
#else
#define LOGTRACE(...)     SPDLOGWRAPPER(spdlog::level::trace, __VA_ARGS__)
#define LOGDEBUG(...)  	  SPDLOGWRAPPER(spdlog::level::debug, __VA_ARGS__)
#endif
#define LOGINFO(...)      SPDLOGWRAPPER(spdlog::level::info, __VA_ARGS__)
#define LOGWARN(...)      SPDLOGWRAPPER(spdlog::level::warn, __VA_ARGS__)
#define LOGERROR(...)     SPDLOGWRAPPER(spdlog::level::err, __VA_ARGS__)
#define LOGCRITICAL(...)  SPDLOGWRAPPER(spdlog::level::critical, __VA_ARGS__)


//===========================================================================
//
//	ログ
//
//===========================================================================
class Log
{
public:
	enum loglevel {
		Detail,							// 詳細レベル
		Normal,							// 通常レベル
		Warning,						// 警告レベル
		Debug							// デバッグレベル
	};
};

#endif	// log_h
