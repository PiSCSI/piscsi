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
#include "hal/gpiobus_virtual.h"
#include "hal/sbc_version.h"
#include "log.h"

using namespace std;

 
unique_ptr<GPIOBUS> GPIOBUS_Factory::Create(BUS::mode_e mode, board_type::rascsi_board_type_e board)
{
    unique_ptr<GPIOBUS> return_ptr;
    if (SBC_Version::IsBananaPi()) {
        LOGTRACE("Creating GPIOBUS_Allwinner")
        return_ptr =  make_unique<GPIOBUS_Allwinner>();
    } else if (SBC_Version::IsRaspberryPi()) {
        LOGTRACE("Creating GPIOBUS_Raspberry")
        return_ptr = make_unique<GPIOBUS_Raspberry>();
    }
    else {
        LOGINFO("Creating Virtual GPIOBUS");
        return_ptr = make_unique<GPIOBUS_Virtual>();
    }
    return_ptr->Init(mode, board);
    return_ptr->Reset();
    return return_ptr;
}
