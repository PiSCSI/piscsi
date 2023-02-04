//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/log.h"
#include "device_logger.h"

using namespace std;

void DeviceLogger::Trace(const string& message) const
{
	if (const string m = GetLogMessage(message); !m.empty()) {
		LOGTRACE("%s", m.c_str())
	}
}

void DeviceLogger::Debug(const string& message) const
{
	if (const string m = GetLogMessage(message); !m.empty()) {
		LOGDEBUG("%s", m.c_str())
	}
}

void DeviceLogger::Info(const string& message) const
{
	if (const string m = GetLogMessage(message); !m.empty()) {
		LOGINFO("%s", m.c_str())
	}
}

void DeviceLogger::Warn(const string& message) const
{
	if (const string m = GetLogMessage(message); !m.empty()) {
		LOGWARN("%s", m.c_str())
	}
}

void DeviceLogger::Error(const string& message) const
{
	if (const string m = GetLogMessage(message); !m.empty()) {
		LOGERROR("%s", m.c_str())
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
