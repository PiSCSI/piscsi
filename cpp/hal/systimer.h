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

#include <memory>
#include <stdint.h>

#include "shared/scsi.h"

class PlatformSpecificTimer
{
  public:
    // Default constructor
    PlatformSpecificTimer() = default;
    // Default destructor
    virtual ~PlatformSpecificTimer() = default;
    // Initialization
    virtual void Init() = 0;
    // Get system timer low byte
    virtual uint32_t GetTimerLow() = 0;
    // Sleep for N nanoseconds
    virtual void SleepNsec(uint32_t nsec) = 0;
    // Sleep for N microseconds
    virtual void SleepUsec(uint32_t usec) = 0;
};

//===========================================================================
//
//	System timer
//
//===========================================================================
class SysTimer
{
  public:
    static void Init();
    // Get system timer low byte
    static uint32_t GetTimerLow();
    // Sleep for N nanoseconds
    static void SleepNsec(uint32_t nsec);
    // Sleep for N microseconds
    static void SleepUsec(uint32_t usec);

  private:
    static bool initialized;
    static bool is_raspberry;

    static std::unique_ptr<PlatformSpecificTimer> systimer_ptr;
};
