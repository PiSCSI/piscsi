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
			LOGTRACE("(ID %d) - %s", id, error.c_str())
		}
		else {
			LOGTRACE("(ID:LUN %d:%d) - %s", id, lun, error.c_str())
		}
	}
}

void DeviceLogger::Debug(const string& error) const
{
	if (IsLogDevice()) {
		if (lun == -1) {
			LOGDEBUG("(ID %d) - %s", id, error.c_str())
		}
		else {
			LOGDEBUG("(ID:LUN %d:%d) - %s", id, lun, error.c_str())
		}
	}
}

void DeviceLogger::Info(const string& error) const
{
	if (IsLogDevice()) {
		if (lun == -1) {
			LOGINFO("(ID %d) - %s", id, error.c_str())
		}
		else {
			LOGINFO("(ID:LUN %d:%d) - %s", id, lun, error.c_str())
		}
	}
}

void DeviceLogger::Warn(const string& error) const
{
	if (IsLogDevice()) {
		if (lun == -1) {
			LOGWARN("(ID %d) - %s", id, error.c_str())
		}
		else {
			LOGWARN("(ID:LUN %d:%d) - %s", id, lun, error.c_str())
		}
	}
}

void DeviceLogger::Error(const string& error) const
{
	if (IsLogDevice()) {
		if (lun == -1) {
			LOGERROR("(ID %d) - %s", id, error.c_str())
		}
		else {
			LOGERROR("(ID:LUN %d:%d) - %s", id, lun, error.c_str())
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
