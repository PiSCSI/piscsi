//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/gpiobus.h"
#include <memory>

class GPIOBUS_Factory
{
  public:

  static unique_ptr<GPIOBUS> Create(BUS::mode_e mode);
};
