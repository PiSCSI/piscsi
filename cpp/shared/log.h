//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2020 akuker
//
//---------------------------------------------------------------------------

#pragma once

#include "spdlog/spdlog.h"
#include <array>
#include <string>

using namespace std;

static const int LOGBUF_SIZE = 512;

#define SPDLOGWRAPPER(loglevel, ...)                   \
    {                                                  \
        char logbuf[LOGBUF_SIZE];                      \
        snprintf(logbuf, sizeof(logbuf), __VA_ARGS__); \
        spdlog::log(loglevel, logbuf);                 \
    };

#define LOGTRACE(...) SPDLOGWRAPPER(spdlog::level::trace, __VA_ARGS__)
#define LOGDEBUG(...) SPDLOGWRAPPER(spdlog::level::debug, __VA_ARGS__)
#define LOGINFO(...) SPDLOGWRAPPER(spdlog::level::info, __VA_ARGS__)
#define LOGWARN(...) SPDLOGWRAPPER(spdlog::level::warn, __VA_ARGS__)
#define LOGERROR(...) SPDLOGWRAPPER(spdlog::level::err, __VA_ARGS__)

class piscsi_log_level
{
  public:
    static bool set_log_level(const string&);
    static spdlog::level::level_enum get_log_level()
    {
        return current_log_level;
    };
    static const spdlog::string_view_t& get_log_level_str()
    {
        return spdlog_names[current_log_level];
    };

  private:
    static spdlog::level::level_enum current_log_level;
    static std::array<spdlog::string_view_t, (int)spdlog::level::level_enum::n_levels> spdlog_names;
};
