//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "device_logger.h"

using namespace std;
using namespace spdlog;

void DeviceLogger::Trace(const string& message) const
{
	Log(level::trace, message);
}

void DeviceLogger::Debug(const string& message) const
{
	Log(level::debug, message);
}

void DeviceLogger::Info(const string& message) const
{
	Log(level::info, message);
}

void DeviceLogger::Warn(const string& message) const
{
	Log(level::warn, message);
}

void DeviceLogger::Error(const string& message) const
{
	Log(level::err, message);
}

void DeviceLogger::Log(level::level_enum level, const string& message) const
{
    if ((log_device_id == -1 || log_device_id == id) && (lun == -1 || log_device_lun == -1 || log_device_lun == lun)) {
        if (lun == -1) {
            log(level, "(ID " + to_string(id) + ") - " + message);
        }
        else {
            log(level, "(ID:LUN " + to_string(id) + ":" + to_string(lun) + ") - " + message);
        }
    }
}

void DeviceLogger::SetIdAndLun(int i, int l)
{
	id = i;
	lun = l;
}

void DeviceLogger::SetLogIdAndLun(int i, int l)
{
	log_device_id = i;
	log_device_lun = l;
}
