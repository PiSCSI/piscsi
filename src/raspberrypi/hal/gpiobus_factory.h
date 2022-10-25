//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ GPIO-SCSI bus ]
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>

#include "gpiobus.h"

using namespace std;

class GPIOBUS_Factory
{
  public:
    static unique_ptr<GPIOBUS> Create();
};
