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

bool AbstractController::AddDevice(PrimaryDevice *device)
{
	if (device->GetLun() >= GetMaxLuns()) {
		return false;
	}

	if (HasDeviceForLun(device->GetLun())) {
		return false;
	}

	ctrl.luns[device->GetLun()] = device;
	device->SetController(this);

	return true;
}

bool AbstractController::DeleteDevice(const PrimaryDevice *device)
{
	return ctrl.luns.erase(device->GetLun()) == 1;
}

bool AbstractController::HasDeviceForLun(int lun) const
{
	return ctrl.luns.find(lun) != ctrl.luns.end();
}

int AbstractController::ExtractInitiatorId(int id_data) const
{
	int initiator_id = -1;

	if (int tmp = id_data - (1 << target_id); tmp) {
		initiator_id = 0;
		for (int j = 0; j < 8; j++) {
			tmp >>= 1;
			if (tmp) {
				initiator_id++;
			}
			else {
				break;
			}
		}
	}

	return initiator_id;
}
