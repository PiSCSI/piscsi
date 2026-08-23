//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2026 Daniel Markstedt <daniel@mindani.net>
//
//---------------------------------------------------------------------------

#include "controllers/controller_manager.h"
#include "sasi_controller.h"

SasiController::SasiController(BUS& bus, int target_id)
	: ScsiController(bus, target_id, ControllerManager::GetSasiLunMax())
{
}

int SasiController::GetEffectiveLun() const
{
	return GetLun();
}

void SasiController::MsgOut()
{
	// SASI does not define a message out phase.
	Command();
}
