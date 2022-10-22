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

class PlatformSpecificTimer
{
public:
    virtual void Init() = 0;
    // Get system timer low byte
    virtual uint32_t GetTimerLow() = 0;
    // Get system timer high byte
    virtual uint32_t GetTimerHigh() = 0;
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
    // Get system timer high byte
    static uint32_t GetTimerHigh();
    // Sleep for N nanoseconds
    static void SleepNsec(uint32_t nsec);
    // Sleep for N microseconds
    static void SleepUsec(uint32_t usec);

private:
    static bool initialized;
    static bool is_allwinnner;
    static bool is_raspberry;

    static std::unique_ptr<PlatformSpecificTimer> systimer_ptr;
};
