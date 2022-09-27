//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "devices/device_factory.h"
#include "devices/file_support.h"
#include "controllers/controller_manager.h"
#include "rascsi_executor.h"

void RascsiExecutor::DetachAll()
{
	controller_manager.DeleteAllControllers();
	device_factory.DeleteAllDevices();
	FileSupport::UnreserveAll();

	LOGINFO("Detached all devices")
}
