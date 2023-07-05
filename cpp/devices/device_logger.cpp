//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "spdlog/spdlog.h"
#include "device_logger.h"

using namespace std;

void DeviceLogger::Trace(const string& message) const
{
	if (const string m = GetLogMessage(message); !m.empty()) {
		spdlog::trace(m);
	}
}

void DeviceLogger::Debug(const string& message) const
{
	if (const string m = GetLogMessage(message); !m.empty()) {
		spdlog::debug(m);
	}
}

void DeviceLogger::Info(const string& message) const
{
	if (const string m = GetLogMessage(message); !m.empty()) {
		spdlog::info(m);
	}
}

void DeviceLogger::Warn(const string& message) const
{
	if (const string m = GetLogMessage(message); !m.empty()) {
		spdlog::warn(m);
	}
}

void DeviceLogger::Error(const string& message) const
{
	if (const string m = GetLogMessage(message); !m.empty()) {
		spdlog::error(m);
	}
}

string DeviceLogger::GetLogMessage(const string& message) const
{
	if (log_device_id == -1 || (log_device_id == id && (log_device_lun == -1 || log_device_lun == lun)))
	{
		if (lun == -1) {
			return "(ID " + to_string(id) + ") - " + message;
		}
		else {
			return "(ID:LUN " + to_string(id) + ":" + to_string(lun) + ") - " + message;
		}
	}

	return "";
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
