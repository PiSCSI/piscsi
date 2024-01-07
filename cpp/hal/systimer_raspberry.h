//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
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
    ~SysTimer_Raspberry() override = default;
    // Initialization
    void Init() override;
    // Get system timer low byte
    uint32_t GetTimerLow() override;
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

    const static int ARMT_LOAD    = 0;
    const static int ARMT_VALUE   = 1;
    const static int ARMT_CTRL    = 2;
    const static int ARMT_CLRIRQ  = 3;
    const static int ARMT_RAWIRQ  = 4;
    const static int ARMT_MSKIRQ  = 5;
    const static int ARMT_RELOAD  = 6;
    const static int ARMT_PREDIV  = 7;
    const static int ARMT_FREERUN = 8;

    const static int SYST_CS  = 0;
    const static int SYST_CLO = 1;
    const static int SYST_CHI = 2;
    const static int SYST_C0  = 3;
    const static int SYST_C1  = 4;
    const static int SYST_C2  = 5;
    const static int SYST_C3  = 6;

    const static uint32_t SYST_OFFSET = 0x00003000;
    const static uint32_t ARMT_OFFSET = 0x0000B400;
};
