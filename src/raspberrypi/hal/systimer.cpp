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
#include <sys/mman.h>

#include "os.h"
#include "hal/gpiobus.h"

#include "config.h"
#include "log.h"


	static std::shared_ptr<SysTimer> private_instance = nullptr;


ControllerManager& ControllerManager::instance()
{
	// If we haven't set up the private instance yet, do that now
	if(private_instance == nullptr){
		if(SBC_Version::IsRaspberryPi()){
			private_instance = std::make_shared<SysTimer_Raspberry>();
			private_instance->Init();
		}
		else if(SBC_Version::IsBananaPi()){
			private_instance = std::make_shared<SysTimer_AllWinner>();
			private_instance->Init();
		}
	}
	return private_instance;
}



//---------------------------------------------------------------------------
//
//	Sleep in microseconds
//
//---------------------------------------------------------------------------
void SysTimer::instance.SleepUsec(DWORD usec)
{
	// If time is 0, don't do anything
	if (usec == 0) {
		return;
	}

	DWORD now = GetTimerLow();
	while ((GetTimerLow() - now) < usec);
}
