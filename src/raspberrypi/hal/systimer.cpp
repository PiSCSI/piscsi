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
#include "hal/gpiobus.h"
#include "config.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <array>

//---------------------------------------------------------------------------
//
//	System timer address
//
//---------------------------------------------------------------------------
volatile uint32_t* SysTimer::systaddr;

//---------------------------------------------------------------------------
//
//	ARM timer address
//
//---------------------------------------------------------------------------
volatile uint32_t* SysTimer::armtaddr;

//---------------------------------------------------------------------------
//
//	Core frequency
//
//---------------------------------------------------------------------------
volatile DWORD SysTimer::corefreq;

//---------------------------------------------------------------------------
//
//	Initialize the system timer
//
//---------------------------------------------------------------------------
void SysTimer::Init(uint32_t *syst, uint32_t *armt)
{
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
	array<uint32_t, 32> maxclock = { 32, 0, 0x00030004, 8, 0, 4, 0, 0 };

	// Save the base address
	systaddr = syst;
	armtaddr = armt;

	// Change the ARM timer to free run mode
	armtaddr[ARMT_CTRL] = 0x00000282;

	// Get the core frequency
	corefreq = 0;
	int fd = open("/dev/vcio", O_RDONLY);
	if (fd >= 0) {
		ioctl(fd, _IOWR(100, 0, char *), maxclock);
		corefreq = maxclock[6] / 1000000;
	}
	close(fd);
}

//---------------------------------------------------------------------------
//
//	Get system timer low byte
//
//---------------------------------------------------------------------------
uint32_t SysTimer::GetTimerLow() {
	return systaddr[SYST_CLO];
}

//---------------------------------------------------------------------------
//
//	Get system timer high byte
//
//---------------------------------------------------------------------------
uint32_t SysTimer::GetTimerHigh() {
	return systaddr[SYST_CHI];
}

//---------------------------------------------------------------------------
//
//	Sleep in nanoseconds
//
//---------------------------------------------------------------------------
void SysTimer::SleepNsec(DWORD nsec)
{
	// If time is 0, don't do anything
	if (nsec == 0) {
		return;
	}

	// Calculate the timer difference
	uint32_t diff = corefreq * nsec / 1000;

	// Return if the difference in time is too small
	if (diff == 0) {
		return;
	}

	// Start
	uint32_t start = armtaddr[ARMT_FREERUN];

	// Loop until timer has elapsed
	while ((armtaddr[ARMT_FREERUN] - start) < diff);
}

//---------------------------------------------------------------------------
//
//	Sleep in microseconds
//
//---------------------------------------------------------------------------
void SysTimer::SleepUsec(DWORD usec)
{
	// If time is 0, don't do anything
	if (usec == 0) {
		return;
	}

	uint32_t now = GetTimerLow();
	while ((GetTimerLow() - now) < usec);
}
