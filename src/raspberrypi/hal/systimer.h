//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2022 akuker
//
//	[ High resolution timer ]
//
//---------------------------------------------------------------------------

#pragma once

#include <memory>
#include <stdint.h>

#include "config.h"
#include "scsi.h"

//===========================================================================
//
//	System timer
//
//===========================================================================
class SysTimer
{
public:
	virtual uint32_t GetTimerLow();
										// Get system timer low byte
	virtual uint32_t GetTimerHigh();
										// Get system timer high byte
	virtual void SleepNsec(uint32_t nsec);
										// Sleep for N nanoseconds
	virtual void SleepUsec(uint32_t usec);
										// Sleep for N microseconds
	static SysTimer& instance();

private:
	static std::shared_ptr<SysTimer> private_instance;
};
