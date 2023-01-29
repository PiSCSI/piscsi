//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//  Copyright (C) 2023 akuker
//
//	[ OS-based timer for simulation purposes ]
//
//---------------------------------------------------------------------------
#pragma once

#include "systimer.h"
#include <stdint.h>
#include <string>

//===========================================================================
//
//	System timer
//
//===========================================================================
class SysTimer_Linux : public PlatformSpecificTimer
{
  public:
    // Default constructor
    SysTimer_Linux() = default;
    // Default destructor
    ~SysTimer_Linux() override = default;
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
    struct timespec timer_resolution;
};
