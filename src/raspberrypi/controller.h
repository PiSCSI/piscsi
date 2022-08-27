//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Interface for controllers
//
//---------------------------------------------------------------------------

#pragma once

#include "phase_handler.h"

class Controller : virtual public PhaseHandler
{
public:

	Controller() {}
	virtual ~Controller() {}


};
