//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/log.h"
#include "device_logger.h"

using namespace std;

void DeviceLogger::Trace(const string& error) const
{
	if (log_device_id == -1 || (log_device_id == id && (log_device_lun == -1 || log_device_lun == lun))) {
		LOGTRACE("(%d:%d) - %s", id, lun, error.c_str())
	}
}

void DeviceLogger::Debug(const string& error) const
{
	if (log_device_id == -1 || (log_device_id == id && (log_device_lun == -1 || log_device_lun == lun))) {
		LOGDEBUG("(%d:%d) - %s", id, lun, error.c_str())
	}
}

void DeviceLogger::Info(const string& error) const
{
	if (log_device_id == -1 || (log_device_id == id && (log_device_lun == -1 || log_device_lun == lun))) {
		LOGINFO("(%d:%d) - %s", id, lun, error.c_str())
	}
}

void DeviceLogger::Warn(const string& error) const
{
	if (log_device_id == -1 || (log_device_id == id && (log_device_lun == -1 || log_device_lun == lun))) {
		LOGWARN("(%d:%d) - %s", id, lun, error.c_str())
	}
}

void DeviceLogger::Error(const string& error) const
{
	if (log_device_id == -1 || (log_device_id == id && (log_device_lun == -1 || log_device_lun == lun))) {
		LOGERROR("(%d:%d) - %s", id, lun, error.c_str())
	}
}
