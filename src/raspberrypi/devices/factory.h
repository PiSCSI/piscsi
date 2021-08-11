//---------------------------------------------------------------------------
//
//      SCSI Target Emulator RaSCSI (*^..^*)
//      for Raspberry Pi
//
//      Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include "rascsi_interface.pb.h"

class Device;

class DeviceFactory
{
public:

	DeviceFactory() { };
	~DeviceFactory() { };

	static Device *CreateDevice(rascsi_interface::PbDeviceType& type, const std::string& ext);
};
