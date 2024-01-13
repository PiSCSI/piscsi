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

#include "hal/systimer.h"
#include "hal/systimer_raspberry.h"
#include <spdlog/spdlog.h>

#include "hal/gpiobus.h"
#include "hal/sbc_version.h"

bool SysTimer::initialized = false;
bool SysTimer::is_raspberry = false;

using namespace std;

unique_ptr<PlatformSpecificTimer> SysTimer::systimer_ptr;

void SysTimer::Init()
{
    if (!initialized) {
        if (SBC_Version::IsRaspberryPi()) {
            systimer_ptr = make_unique<SysTimer_Raspberry>();
            is_raspberry = true;
        }
        systimer_ptr->Init();
        initialized = true;
    }
}

// Get system timer low byte
uint32_t SysTimer::GetTimerLow()
{
    return systimer_ptr->GetTimerLow();
}

// Sleep for N nanoseconds
void SysTimer::SleepNsec(uint32_t nsec)
{
    systimer_ptr->SleepNsec(nsec);
}

// Sleep for N microseconds
void SysTimer::SleepUsec(uint32_t usec)
{
    systimer_ptr->SleepUsec(usec);
}
