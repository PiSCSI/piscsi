//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//  Copyright (C) 2023 akuker
//
//	[ High resolution timer for the Allwinner series of SoC's]
//
//---------------------------------------------------------------------------

#include "hal/systimer_linux.h"
#include <sys/mman.h>

#include "hal/gpiobus.h"

#include "shared/log.h"

//---------------------------------------------------------------------------
//
//	Initialize the system timer
//
//---------------------------------------------------------------------------
void SysTimer_Linux::Init()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__)

    int result = clock_getres(CLOCK_PROCESS_CPUTIME_ID, &timer_resolution);
    LOGINFO("CPU resolution is: %lu (result: %d)", timer_resolution.tv_nsec, result);
}

uint32_t SysTimer_Linux::GetTimerLow()
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    return 0;
}

uint32_t SysTimer_Linux::GetTimerHigh()
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    return (uint32_t)0;
}

//---------------------------------------------------------------------------
//
//	Sleep in nanoseconds
//
//---------------------------------------------------------------------------
void SysTimer_Linux::SleepNsec(uint32_t nsec)
{
    (void)nsec;
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);

    //     // If time is less than one HS timer clock tick, don't do anything
    //     if (nsec < 20) {
    //         return;
    //     }

    //     // The HS timer receives a 200MHz clock input, which equates to
    //     // one clock tick every 5 ns.
    //     auto clockticks = (uint32_t)std::ceil(nsec / 5);

    //     uint32_t enter_time = hsitimer_regs->hs_tmr_curnt_lo_reg;

    //     // TODO: NEED TO HANDLE COUNTER OVERFLOW
    //     LOGTRACE("%s entertime: %08X ns: %d clockticks: %d", __PRETTY_FUNCTION__, enter_time, nsec, clockticks)
    //     while ((enter_time - hsitimer_regs->hs_tmr_curnt_lo_reg) < clockticks)
    //         ;

    return;
}

//---------------------------------------------------------------------------
//
//	Sleep in microseconds
//
//---------------------------------------------------------------------------
void SysTimer_Linux::SleepUsec(uint32_t usec)
{
    (void)usec;
    LOGTRACE("%s", __PRETTY_FUNCTION__)
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);

    // // If time is 0, don't do anything
    // if (usec == 0) {
    //     return;
    // }

    // uint32_t enter_time = GetTimerLow();
    // while ((GetTimerLow() - enter_time) < usec)
    //     ;
}
