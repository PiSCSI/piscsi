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

#include "os.h"
#include "hal/gpiobus.h"
#include "hal/sbc_version.h"

#include "config.h"
#include "log.h"

bool SysTimer::initialized = false;
bool SysTimer::is_allwinnner = false;
bool SysTimer::is_raspberry = false;

SysTimer& SysTimer::instance()
{
	LOGTRACE("%s", __PRETTY_FUNCTION__);
	static SysTimer_AllWinner allwinner_instance;
	static SysTimer_Raspberry raspberry_instance;

	if(!initialized){
		if(SBC_Version::IsRaspberryPi()){
			raspberry_instance.Init();
			is_raspberry = true;
		}
		else if(SBC_Version::IsBananaPi()){
			allwinner_instance.Init();
			is_allwinnner = true;
		}
		initialized = true;
	}
	if(is_allwinnner){
		return allwinner_instance;
	}else{
		return raspberry_instance;
	}
}

