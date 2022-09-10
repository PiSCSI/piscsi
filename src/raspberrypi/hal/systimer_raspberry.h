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
	override void Init(uint32_t *syst, uint32_t *armt);
										// Initialization
	override uint32_t GetTimerLow();
										// Get system timer low byte
	override uint32_t GetTimerHigh();
										// Get system timer high byte
	override void SleepNsec(uint32_t nsec);
										// Sleep for N nanoseconds
	override void SleepUsec(uint32_t usec);
										// Sleep for N microseconds

private:
	static volatile uint32_t *systaddr;
										// System timer address
	static volatile uint32_t *armtaddr;
										// ARM timer address
	static volatile uint32_t corefreq = 0;
										// Core frequency
};
