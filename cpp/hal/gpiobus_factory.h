//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#pragma once

#include "bus.h"
#include <memory>

using namespace std;

class GPIOBUS_Factory
{
  public:

    static unique_ptr<BUS> Create(BUS::mode_e);
};
