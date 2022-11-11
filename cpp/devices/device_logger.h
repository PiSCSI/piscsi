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

	string GetLogMessage(const string&) const;

	void SetIdAndLun(int, int);

	static void SetLogIdAndLun(int, int);

private:

	int id = -1;
	int lun = -1;

	// TODO Try to only have one shared instance, so that these fields do not have to be static
	static inline int log_device_id = -1;
	static inline int log_device_lun = -1;
};
