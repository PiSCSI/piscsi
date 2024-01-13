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

#include "hal/systimer_raspberry.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "hal/gpiobus.h"
#include "hal/sbc_version.h"

// System timer address
volatile uint32_t *SysTimer_Raspberry::systaddr = nullptr;
// ARM timer address
volatile uint32_t *SysTimer_Raspberry::armtaddr = nullptr;
volatile uint32_t SysTimer_Raspberry::corefreq  = 0;

using namespace std;

//---------------------------------------------------------------------------
//
//	Initialize the system timer
//
//---------------------------------------------------------------------------
void SysTimer_Raspberry::Init()
{
    // Get the base address
    const auto baseaddr = SBC_Version::GetPeripheralAddress();

    // Open /dev/mem
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd == -1) {
        spdlog::error("Error: Unable to open /dev/mem. Are you running as root?");
        return;
    }

    // Map peripheral region memory
    void *map = mmap(nullptr, 0x1000100, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, baseaddr);
    if (map == MAP_FAILED) {
        spdlog::error("Error: Unable to map memory");
        close(mem_fd);
        return;
    }
    close(mem_fd);

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
    const array<uint32_t, 32> maxclock = {32, 0, 0x00030004, 8, 0, 4, 0, 0};

    // Save the base address
    systaddr = (uint32_t *)map + SYST_OFFSET / sizeof(uint32_t);
    armtaddr = (uint32_t *)map + ARMT_OFFSET / sizeof(uint32_t);

    // Change the ARM timer to free run mode
    armtaddr[ARMT_CTRL] = 0x00000282;

    // Get the core frequency
    if (int vcio_fd = open("/dev/vcio", O_RDONLY); vcio_fd >= 0) {
        ioctl(vcio_fd, _IOWR(100, 0, char *), maxclock.data());
        corefreq = maxclock[6] / 1000000;
        close(vcio_fd);
    }
}

//---------------------------------------------------------------------------
//
//	Get system timer low byte
//
//---------------------------------------------------------------------------
uint32_t SysTimer_Raspberry::GetTimerLow()
{
    return systaddr[SYST_CLO];
}

//---------------------------------------------------------------------------
//
//	Sleep in nanoseconds
//
//---------------------------------------------------------------------------
void SysTimer_Raspberry::SleepNsec(uint32_t nsec)
{
    // If time is 0, don't do anything
    if (nsec == 0) {
        return;
    }

    // Calculate the timer difference
    const uint32_t diff = corefreq * nsec / 1000;

    // Return if the difference in time is too small
    if (diff == 0) {
        return;
    }

    // Start
    const uint32_t start = armtaddr[ARMT_FREERUN];

    // Loop until timer has elapsed
    while ((armtaddr[ARMT_FREERUN] - start) < diff)
        ;
}

//---------------------------------------------------------------------------
//
//	Sleep in microseconds
//
//---------------------------------------------------------------------------
void SysTimer_Raspberry::SleepUsec(uint32_t usec)
{
    // If time is 0, don't do anything
    if (usec == 0) {
        return;
    }

    const uint32_t now = GetTimerLow();
    while ((GetTimerLow() - now) < usec)
        ;
}
