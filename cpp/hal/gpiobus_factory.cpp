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

#include "hal/gpiobus_bananam2p.h"
#include "hal/gpiobus_factory.h"
#include "hal/gpiobus_raspberry.h"
// #include "hal/gpiobus_virtual.h"
#include "hal/sbc_version.h"
#include "shared/log.h"

using namespace std;

 
unique_ptr<GPIOBUS> GPIOBUS_Factory::Create(BUS::mode_e mode)
{
    unique_ptr<GPIOBUS> return_ptr;
    if (SBC_Version::IsBananaPi()) {
        LOGTRACE("Creating GPIOBUS_BananaM2p")
        return_ptr =  make_unique<GPIOBUS_BananaM2p>();
    } else if (SBC_Version::IsRaspberryPi()) {
        LOGTRACE("Creating GPIOBUS_Raspberry")
        return_ptr = make_unique<GPIOBUS_Raspberry>();
    }
    else {
        return nullptr;
        // LOGINFO("Creating Virtual GPIOBUS");
        // return_ptr = make_unique<GPIOBUS_Virtual>();
    }
    return_ptr->Init(mode);
    return_ptr->Reset();
    return return_ptr;
}
