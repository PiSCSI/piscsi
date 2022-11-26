//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/bus.h"
#include <memory>

class GPIOBUS_Factory
{
  public:

  static unique_ptr<BUS> Create(BUS::mode_e mode);
};
