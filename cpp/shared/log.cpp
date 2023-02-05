//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2020-2023 akuker
//
//---------------------------------------------------------------------------

#include "log.h"
#include "spdlog/spdlog.h"

using namespace spdlog;

level::level_enum piscsi_log_level::current_log_level = level::info;

std::array<string_view_t, (int)level::level_enum::n_levels> piscsi_log_level::spdlog_names = SPDLOG_LEVEL_NAMES;

bool piscsi_log_level::set_log_level(const string& log_level)
{
    if (log_level == "trace") {
        current_log_level = level::trace;
    } else if (log_level == "debug") {
        current_log_level = level::debug;
    } else if (log_level == "info") {
        current_log_level = level::info;
    } else if (log_level == "warn") {
        current_log_level = level::warn;
    } else if (log_level == "err") {
        current_log_level = level::err;
    } else if (log_level == "critical") {
        current_log_level = level::critical;
    } else if (log_level == "off") {
        current_log_level = level::off;
    } else {
        return false;
    }

    set_level(current_log_level);

    LOGINFO("Set log level to '%s'", get_log_level_str().data())

    return true;
}