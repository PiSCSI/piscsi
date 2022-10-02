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
class SysTimer_Raspberry : public SysTimer
{
public:
	// Initialization
	void Init();
	// Get system timer low byte
	uint32_t GetTimerLow();
	// Get system timer high byte
	uint32_t GetTimerHigh();
	// Sleep for N nanoseconds
	void SleepNsec(uint32_t nsec);
	// Sleep for N microseconds
	void SleepUsec(uint32_t usec);

private:
	// System timer address
	static volatile uint32_t *systaddr;
	// ARM timer address
	static volatile uint32_t *armtaddr;
	// Core frequency
	static volatile uint32_t corefreq;
};
