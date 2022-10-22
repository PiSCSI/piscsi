//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//	[ GPIO bus factory ]
//
//---------------------------------------------------------------------------

#include <memory>

#include "hal/gpiobus_allwinner.h"
#include "hal/gpiobus_factory.h"
#include "hal/gpiobus_raspberry.h"
#include "hal/sbc_version.h"
#include "log.h"

using namespace std;

unique_ptr<GPIOBUS> GPIOBUS_Factory::Create()
{
    if (SBC_Version::IsBananaPi()) {
        LOGTRACE("Creating GPIOBUS_Allwinner")
        return make_unique<GPIOBUS_Allwinner>();
    } else {
        LOGTRACE("Creating GPIOBUS_Raspberry")
        return make_unique<GPIOBUS_Raspberry>();
    }
}
