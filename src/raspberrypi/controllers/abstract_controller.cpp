//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "abstract_controller.h"
#include "devices/primary_device.h"

PrimaryDevice *AbstractController::GetDeviceForLun(int lun) const {
	const auto& it = ctrl.luns.find(lun);
	return it == ctrl.luns.end() ? nullptr : it->second;
}

void AbstractController::SetDeviceForLun(int lun, PrimaryDevice *device)
{
	assert(lun < LUN_MAX);

	delete GetDeviceForLun(lun);

	if (device != nullptr) {
		ctrl.luns[lun] = device;
		device->SetController(this);
	}
	else {
		ctrl.luns.erase(lun);
	}
}

bool AbstractController::HasDeviceForLun(int lun) const
{
	return ctrl.luns.find(lun) != ctrl.luns.end();
}

void AbstractController::ClearLuns()
{
	ctrl.luns.clear();
}
