//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include "spdlog/spdlog.h"

static const int LOGBUF_SIZE = 512;

#define SPDLOGWRAPPER(loglevel, ...)			\
{							\
	char logbuf[LOGBUF_SIZE];				\
	snprintf(logbuf, sizeof(logbuf), __VA_ARGS__);	\
	spdlog::log(loglevel, logbuf);			\
};

#define LOGTRACE(...) SPDLOGWRAPPER(spdlog::level::trace, __VA_ARGS__)
#define LOGDEBUG(...) SPDLOGWRAPPER(spdlog::level::debug, __VA_ARGS__)
#define LOGINFO(...) SPDLOGWRAPPER(spdlog::level::info, __VA_ARGS__)
#define LOGWARN(...) SPDLOGWRAPPER(spdlog::level::warn, __VA_ARGS__)
#define LOGERROR(...) SPDLOGWRAPPER(spdlog::level::err, __VA_ARGS__)

inline std::string strerrno(const std::string& msg) {
	return msg + ": " + std::string(strerror(errno));
}
