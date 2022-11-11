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
		if (lun == -1) {
			LOGTRACE("(%d) - %s", id, error.c_str())
		}
		else {
			LOGTRACE("(%d:%d) - %s", id, lun, error.c_str())
		}
	}
}

void DeviceLogger::Debug(const string& error) const
{
	if (IsLogDevice()) {
		if (lun == -1) {
			LOGDEBUG("(%d) - %s", id, error.c_str())
		}
		else {
			LOGDEBUG("(%d:%d) - %s", id, lun, error.c_str())
		}
	}
}

void DeviceLogger::Info(const string& error) const
{
	if (IsLogDevice()) {
		if (lun == -1) {
			LOGINFO("(%d) - %s", id, error.c_str())
		}
		else {
			LOGINFO("(%d:%d) - %s", id, lun, error.c_str())
		}
	}
}

void DeviceLogger::Warn(const string& error) const
{
	if (IsLogDevice()) {
		if (lun == -1) {
			LOGWARN("(%d) - %s", id, error.c_str())
		}
		else {
			LOGWARN("(%d:%d) - %s", id, lun, error.c_str())
		}
	}
}

void DeviceLogger::Error(const string& error) const
{
	if (IsLogDevice()) {
		if (lun == -1) {
			LOGERROR("(%d) - %s", id, error.c_str())
		}
		else {
			LOGERROR("(%d:%d) - %s", id, lun, error.c_str())
		}
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
