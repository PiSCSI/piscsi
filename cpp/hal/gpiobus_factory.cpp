//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "hal/gpiobus_factory.h"
#include "hal/gpiobus.h"

using namespace std;

unique_ptr<BUS> GPIOBUS_Factory::Create(BUS::mode_e mode)
{
	// TODO Make the factory a friend of GPIOBUS and make the GPIOBUS constructor private
	auto bus = make_unique<GPIOBUS>();
	if (bus->Init(mode)) {
		bus->Reset();

		return bus;
	}

	return nullptr;
}
