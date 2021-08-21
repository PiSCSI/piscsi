//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// A DeviceFactory creates devices based on their type and the extension of their image file
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

	static Device *CreateDevice(rascsi_interface::PbDeviceType& type, const std::string& filename, const std::string& ext);
};
