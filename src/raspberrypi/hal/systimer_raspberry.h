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

#include "systimer.h"
#include <stdint.h>

//===========================================================================
//
//	System timer
//
//===========================================================================
class SysTimer_Raspberry : public PlatformSpecificTimer
{
  public:
    // Default constructor
    SysTimer_Raspberry() = default;
    // Default destructor
    ~SysTimer_Raspberry() = default;
    // Initialization
    void Init() override;
    // Get system timer low byte
    uint32_t GetTimerLow() override;
    // Get system timer high byte
    uint32_t GetTimerHigh() override;
    // Sleep for N nanoseconds
    void SleepNsec(uint32_t nsec) override;
    // Sleep for N microseconds
    void SleepUsec(uint32_t usec) override;

  private:
    // System timer address
    static volatile uint32_t *systaddr;
    // ARM timer address
    static volatile uint32_t *armtaddr;
    // Core frequency
    static volatile uint32_t corefreq;
};
