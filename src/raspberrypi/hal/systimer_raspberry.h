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

#include <stdint.h>
#include "systimer.h"

//===========================================================================
//
//	System timer
//
//===========================================================================
class SysTimer_Raspberry : SysTimer
{
public:
	void Init();
										// Initialization
	uint32_t GetTimerLow();
										// Get system timer low byte
	uint32_t GetTimerHigh();
										// Get system timer high byte
	void SleepNsec(uint32_t nsec);
										// Sleep for N nanoseconds
	void SleepUsec(uint32_t usec);
										// Sleep for N microseconds

private:
	static volatile uint32_t *systaddr;
										// System timer address
	static volatile uint32_t *armtaddr;
										// ARM timer address
	static volatile uint32_t corefreq;
										// Core frequency
};
