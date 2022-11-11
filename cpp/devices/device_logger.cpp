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
	if (IsLogDevice()) {
		LOGTRACE("(%d:%d) - %s", id, lun, error.c_str())
	}
}

void DeviceLogger::Debug(const string& error) const
{
	if (IsLogDevice()) {
		LOGDEBUG("(%d:%d) - %s", id, lun, error.c_str())
	}
}

void DeviceLogger::Info(const string& error) const
{
	if (IsLogDevice()) {
		LOGINFO("(%d:%d) - %s", id, lun, error.c_str())
	}
}

void DeviceLogger::Warn(const string& error) const
{
	if (IsLogDevice()) {
		LOGWARN("(%d:%d) - %s", id, lun, error.c_str())
	}
}

void DeviceLogger::Error(const string& error) const
{
	if (IsLogDevice()) {
		LOGERROR("(%d:%d) - %s", id, lun, error.c_str())
	}
}

bool DeviceLogger::IsLogDevice() const
{
	return log_device_id == -1 || (log_device_id == id && (log_device_lun == -1 || log_device_lun == lun));
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
