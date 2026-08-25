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
	// The X68000 probes for a SASI disk with the non-standard CDB 00 28.
	// Treat it as a LUN 0 command so that the device can return the expected
	// CHECK CONDITION instead of rejecting the encoded LUN as unsupported.
	if (GetOpcode() == scsi_defs::scsi_command::eCmdTestUnitReady && GetCmdByte(1) == 0x28) {
		return 0;
	}

	return GetLun();
}

void SasiController::MsgOut()
{
	// SASI does not define a message-out phase.
	Command();
}
