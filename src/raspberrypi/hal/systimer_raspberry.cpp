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

#include "hal/systimer_raspberry.h"
#include <memory>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "hal/gpiobus.h"
#include "hal/sbc_version.h"
#include "os.h"

#include "config.h"
#include "log.h"

// System timer address
volatile uint32_t *SysTimer_Raspberry::systaddr = 0;
// ARM timer address
volatile uint32_t *SysTimer_Raspberry::armtaddr = 0;
volatile uint32_t SysTimer_Raspberry::corefreq  = 0;

//---------------------------------------------------------------------------
//
//	Initialize the system timer
//
//---------------------------------------------------------------------------
void SysTimer_Raspberry::Init()
{
    // Get the base address
    auto baseaddr = SBC_Version::GetPeripheralAddress();

    // Open /dev/mem
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        LOGERROR("Error: Unable to open /dev/mem. Are you running as root?")
        return;
    }

    // Map peripheral region memory
    void *map = mmap(NULL, 0x1000100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, baseaddr);
    if (map == MAP_FAILED) {
        LOGERROR("Error: Unable to map memory")
        close(fd);
        return;
    }
    close(fd);

    // RPI Mailbox property interface
    // Get max clock rate
    //  Tag: 0x00030004
    //
    //  Request: Length: 4
    //   Value: u32: clock id
    //  Response: Length: 8
    //   Value: u32: clock id, u32: rate (in Hz)
    //
    // Clock id
    //  0x000000004: CORE
    DWORD maxclock[32] = {32, 0, 0x00030004, 8, 0, 4, 0, 0};

    // Save the base address
    systaddr = (DWORD *)map + SYST_OFFSET / sizeof(DWORD);
    armtaddr = (DWORD *)map + ARMT_OFFSET / sizeof(DWORD);

    // Change the ARM timer to free run mode
    armtaddr[ARMT_CTRL] = 0x00000282;

    // Get the core frequency
    if (int fd = open("/dev/vcio", O_RDONLY); fd >= 0) {
        ioctl(fd, _IOWR(100, 0, char *), maxclock);
        corefreq = maxclock[6] / 1000000;
        close(fd);
    }
}

//---------------------------------------------------------------------------
//
//	Get system timer low byte
//
//---------------------------------------------------------------------------
DWORD SysTimer_Raspberry::GetTimerLow()
{
    return systaddr[SYST_CLO];
}

//---------------------------------------------------------------------------
//
//	Get system timer high byte
//
//---------------------------------------------------------------------------
DWORD SysTimer_Raspberry::GetTimerHigh()
{
    return systaddr[SYST_CHI];
}

//---------------------------------------------------------------------------
//
//	Sleep in nanoseconds
//
//---------------------------------------------------------------------------
void SysTimer_Raspberry::SleepNsec(DWORD nsec)
{
    // If time is 0, don't do anything
    if (nsec == 0) {
        return;
    }

    // Calculate the timer difference
    DWORD diff = corefreq * nsec / 1000;

    // Return if the difference in time is too small
    if (diff == 0) {
        return;
    }

    // Start
    DWORD start = armtaddr[ARMT_FREERUN];

    // Loop until timer has elapsed
    while ((armtaddr[ARMT_FREERUN] - start) < diff)
        ;
}

//---------------------------------------------------------------------------
//
//	Sleep in microseconds
//
//---------------------------------------------------------------------------
void SysTimer_Raspberry::SleepUsec(DWORD usec)
{
    // If time is 0, don't do anything
    if (usec == 0) {
        return;
    }

    DWORD now = GetTimerLow();
    while ((GetTimerLow() - now) < usec)
        ;
}
