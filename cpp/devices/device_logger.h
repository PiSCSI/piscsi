//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

using namespace std;

class DeviceLogger
{

public:

	DeviceLogger() = default;
	~DeviceLogger() = default;

	void Trace(const string&) const;
	void Debug(const string&) const;
	void Info(const string&) const;
	void Warn(const string&) const;
	void Error(const string&) const;

	bool IsLogDevice() const;

	void SetIdAndLun(int i, int l) { id = i; lun = l; }

	static void SetLogDevice(int i, int l) { log_device_id = i; log_device_lun = l; }

private:

	int id = -1;
	int lun = -1;

	static inline int log_device_id = -1;
	static inline int log_device_lun = -1;
};
