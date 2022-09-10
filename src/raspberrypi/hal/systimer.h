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
	static void Init(uint32_t *syst, uint32_t *armt);
										// Initialization
	static uint32_t GetTimerLow();
										// Get system timer low byte
	static uint32_t GetTimerHigh();
										// Get system timer high byte
	static void SleepNsec(uint32_t nsec);
										// Sleep for N nanoseconds
	static void SleepUsec(uint32_t usec);
										// Sleep for N microseconds

private:
	static volatile uint32_t *systaddr;
										// System timer address
	static volatile uint32_t *armtaddr;
										// ARM timer address
	static volatile uint32_t corefreq;
										// Core frequency
};
