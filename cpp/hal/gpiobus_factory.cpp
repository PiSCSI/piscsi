//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 akuker
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <memory>

#include "hal/gpiobus_factory.h"
#include "hal/gpiobus_raspberry.h"
#include "hal/gpiobus_virtual.h"
#include "hal/sbc_version.h"
#include <spdlog/spdlog.h>

using namespace std;

unique_ptr<BUS> GPIOBUS_Factory::Create(BUS::mode_e mode)
{
    unique_ptr<BUS> bus;

    try {
        SBC_Version::Init();
        if (SBC_Version::IsRaspberryPi()) {
            spdlog::trace("Creating GPIO bus for Raspberry Pi");
            bus = make_unique<GPIOBUS_Raspberry>();
        } else {
        	spdlog::trace("Creating virtual GPIO bus");
            bus = make_unique<GPIOBUS_Virtual>();
        }

        if (bus->Init(mode)) {
        	bus->Reset();
        }
    } catch (const invalid_argument&) {
        spdlog::error("Exception while trying to initialize GPIO bus. Are you running as root?");
        return nullptr;
    }

    return bus;
}
