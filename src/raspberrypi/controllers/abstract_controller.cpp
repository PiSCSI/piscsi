//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "abstract_controller.h"
#include <unordered_map>

AbstractController::AbstractController(BUS *bus, int scsi_id) {
	this->bus = bus;
	this->device_id = scsi_id;
}

PrimaryDevice *AbstractController::GetDeviceForLun(int lun) const {
	const auto& it = ctrl.luns.find(lun);
	return it == ctrl.luns.end() ? nullptr : it->second;
}

void AbstractController::SetDeviceForLun(int lun, PrimaryDevice *device)
{
	assert(lun < LUN_MAX);

	if (device != nullptr) {
		assert(!HasDeviceForLun(lun));
		ctrl.luns[lun] = device;
	}
	else {
		ctrl.luns.erase(lun);
	}
}

bool AbstractController::HasDeviceForLun(int lun) const {
	return ctrl.luns.find(lun) != ctrl.luns.end();
}
