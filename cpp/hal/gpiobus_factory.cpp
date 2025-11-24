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
#include <unistd.h>
#include <spdlog/spdlog.h>

using namespace std;

unique_ptr<BUS> GPIOBUS_Factory::Create(BUS::mode_e mode)
{
	unique_ptr<BUS> bus;

	try {
		SBC_Version::Init();

		if (SBC_Version::IsRaspberryPi()) {
			if (getuid()) {
				spdlog::error("GPIO bus access requires root permissions. Are you running as root?");
				return nullptr;
			}

			bus = make_unique<GPIOBUS_Raspberry>();
		}
		else {
			bus = make_unique<GPIOBUS_Virtual>();
		}

		if (bus->Init(mode)) {
			bus->Reset();
		}
	}
	catch (const invalid_argument& e) {
		spdlog::error(string("Exception while trying to initialize GPIO bus: ") + e.what());
		return nullptr;
	}

	return bus;
}
