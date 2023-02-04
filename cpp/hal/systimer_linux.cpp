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

// returns microseconds
uint32_t SysTimer_Linux::GetTimerLow()
{
    timespec the_time;

    int res = clock_gettime(CLOCK_MONOTONIC, &the_time);
    if(res == -1){
        char err_string[256];
        char *res = strerror_r(errno, err_string, sizeof(err_string));
        (void)res;
        LOGWARN("%s clock_gettime failed. Errno:%d (%s)", __PRETTY_FUNCTION__, errno, err_string);
    }
    return (uint32_t)(((uint64_t)the_time.tv_sec * 1000 * 1000) + the_time.tv_nsec / 1000);
}

uint32_t SysTimer_Linux::GetTimerHigh()
{
    return GetTimerLow()/1000;
}

//---------------------------------------------------------------------------
//
//	Sleep in nanoseconds
//
//---------------------------------------------------------------------------
void SysTimer_Linux::SleepNsec(uint32_t nsec)
{
    timespec sleep_time = {0, (long)nsec};

    int res = nanosleep(&sleep_time, nullptr);

    if (res == -1) {
        LOGWARN("Nanosleep was interrupted")
    }
    return;
}

//---------------------------------------------------------------------------
//
//	Sleep in microseconds
//
//---------------------------------------------------------------------------
void SysTimer_Linux::SleepUsec(uint32_t usec)
{
    SleepNsec(usec * 1000);
}
