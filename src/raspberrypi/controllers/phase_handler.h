//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// An interface with methods for switching bus phases
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi.h"

class PhaseHandler
{
public:

	PhaseHandler() = default;
	virtual ~PhaseHandler() = default;

	virtual void SetPhase(BUS::phase_t) = 0;
	virtual void BusFree() = 0;
	virtual void Selection() = 0;
	virtual void Command() = 0;
	virtual void Status() = 0;
	virtual void DataIn() = 0;
	virtual void DataOut() = 0;
	virtual void MsgIn() = 0;
	virtual void MsgOut() = 0;
};

