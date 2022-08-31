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

void AbstractController::AddLun(PrimaryDevice *device)
{
	ctrl.luns[device->GetLun()] = device;
	device->SetController(this);
}

void AbstractController::DeleteLun(const PrimaryDevice *device)
{
	ctrl.luns.erase(device->GetLun());
}

bool AbstractController::HasLun(int lun) const
{
	return ctrl.luns.find(lun) != ctrl.luns.end();
}

void AbstractController::ClearLuns()
{
	ctrl.luns.clear();
}

int AbstractController::ExtractInitiatorId(int id_data)
{
	int initiator_id = -1;

	int tmp = id_data - (1 << target_id);
	if (tmp) {
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
