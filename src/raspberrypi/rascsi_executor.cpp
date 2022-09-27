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
#include "devices/primary_device.h"
#include "devices/file_support.h"
#include "rascsi_service.h"
#include "localizer.h"
#include "controllers/controller_manager.h"
#include "command_util.h"
#include "spdlog/spdlog.h"
#include "rascsi_executor.h"

using namespace spdlog;
using namespace command_util;

bool RascsiExecutor::SetLogLevel(const string& log_level)
{
	if (log_level == "trace") {
		set_level(level::trace);
	}
	else if (log_level == "debug") {
		set_level(level::debug);
	}
	else if (log_level == "info") {
		set_level(level::info);
	}
	else if (log_level == "warn") {
		set_level(level::warn);
	}
	else if (log_level == "err") {
		set_level(level::err);
	}
	else if (log_level == "critical") {
		set_level(level::critical);
	}
	else if (log_level == "off") {
		set_level(level::off);
	}
	else {
		return false;
	}

	LOGINFO("Set log level to '%s'", log_level.c_str())

	return true;
}

bool RascsiExecutor::Detach(const CommandContext& context, PrimaryDevice& device, bool dryRun)
{
	// LUN 0 can only be detached if there is no other LUN anymore
	if (!device.GetLun() && controller_manager.FindController(device.GetId())->GetLunCount() > 1) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_LUN0);
	}

	if (!dryRun) {
		// Prepare log string before the device data are lost due to deletion
		string s = "Detached " + device.GetType() + " device with ID " + to_string(device.GetId())
				+ ", unit " + to_string(device.GetLun());

		if (auto file_support = dynamic_cast<FileSupport *>(&device); file_support != nullptr) {
			file_support->UnreserveFile();
		}

		// Delete the existing unit
		service.Lock();
		if (!controller_manager.FindController(device.GetId())->DeleteDevice(device)) {
			service.Unlock();

			return ReturnLocalizedError(context, LocalizationKey::ERROR_DETACH);
		}
		device_factory.DeleteDevice(device);
		service.Unlock();

		LOGINFO("%s", s.c_str())
	}

	return true;
}

void RascsiExecutor::DetachAll()
{
	controller_manager.DeleteAllControllers();
	device_factory.DeleteAllDevices();
	FileSupport::UnreserveAll();

	LOGINFO("Detached all devices")
}
