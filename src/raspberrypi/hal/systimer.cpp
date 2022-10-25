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

#include "hal/systimer.h"
#include "hal/systimer_allwinner.h"
#include "hal/systimer_raspberry.h"
#include <sys/mman.h>

#include "hal/gpiobus.h"
#include "hal/sbc_version.h"
#include "os.h"

#include "config.h"
#include "log.h"

bool SysTimer::initialized   = false;
bool SysTimer::is_allwinnner = false;
bool SysTimer::is_raspberry  = false;

std::unique_ptr<PlatformSpecificTimer> SysTimer::systimer_ptr;

void SysTimer::Init()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__)

    if (!initialized) {
        if (SBC_Version::IsRaspberryPi()) {
            systimer_ptr = make_unique<SysTimer_Raspberry>();
            is_raspberry = true;
        } else if (SBC_Version::IsBananaPi()) {
            systimer_ptr  = make_unique<SysTimer_AllWinner>();
            is_allwinnner = true;
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
// Get system timer high byte
uint32_t SysTimer::GetTimerHigh()
{
    return systimer_ptr->GetTimerHigh();
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
