//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "controllers/controller_manager.h"
#include "controllers/scsi_controller.h"
#include "devices/device_factory.h"
#include "devices/primary_device.h"
#include "devices/disk.h"
#include "devices/file_support.h"
#include "rascsi_service.h"
#include "rascsi_image.h"
#include "rascsi_exceptions.h"
#include "localizer.h"
#include "command_util.h"
#include "spdlog/spdlog.h"
#include "rascsi_executor.h"
#include <sstream>

using namespace spdlog;
using namespace command_util;

bool RascsiExecutor::ProcessCmd(const CommandContext& context, const PbDeviceDefinition& pb_device,
		const PbCommand& command, bool dryRun)
{
	const int id = pb_device.id();
	const int lun = pb_device.unit();
	const PbDeviceType type = pb_device.type();
	const PbOperation operation = command.operation();
	const map<string, string> params = { command.params().begin(), command.params().end() };

	ostringstream s;
	s << (dryRun ? "Validating" : "Executing");
	s << ": operation=" << PbOperation_Name(operation);

	if (!params.empty()) {
		s << ", command params=";
		bool isFirst = true;
		for (const auto& [key, value]: params) {
			if (!isFirst) {
				s << ", ";
			}
			isFirst = false;
			string v = key != "token" ? value : "???";
			s << "'" << key << "=" << v << "'";
		}
	}

	s << ", device id=" << id << ", lun=" << lun << ", type=" << PbDeviceType_Name(type);

	if (pb_device.params_size()) {
		s << ", device params=";
		bool isFirst = true;
		for (const auto& [key, value]: pb_device.params()) {
			if (!isFirst) {
				s << ":";
			}
			isFirst = false;
			s << "'" << key << "=" << value << "'";
		}
	}

	s << ", vendor='" << pb_device.vendor() << "', product='" << pb_device.product()
		<< "', revision='" << pb_device.revision() << "', block size=" << pb_device.block_size();
	LOGINFO("%s", s.str().c_str())

	// Check the Controller Number
	if (id < 0) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_MISSING_DEVICE_ID);
	}
	if (id >= ControllerManager::DEVICE_MAX) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_INVALID_ID, to_string(id), to_string(ControllerManager::DEVICE_MAX - 1));
	}

	if (operation == ATTACH && reserved_ids.find(id) != reserved_ids.end()) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_RESERVED_ID, to_string(id));
	}

	// Check the Unit Number
	if (lun < 0 || lun >= ScsiController::LUN_MAX) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_INVALID_LUN, to_string(lun), to_string(ScsiController::LUN_MAX - 1));
	}

	if (operation == ATTACH) {
		return Attach(context, pb_device, dryRun);
	}

	// Does the controller exist?
	if (!dryRun && controller_manager.FindController(id) == nullptr) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_NON_EXISTING_DEVICE, to_string(id));
	}

	// Does the unit exist?
	PrimaryDevice *device = controller_manager.GetDeviceByIdAndLun(id, lun);
	if (device == nullptr) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_NON_EXISTING_UNIT, to_string(id), to_string(lun));
	}

	if (operation == DETACH) {
		return Detach(context, *device, dryRun);
	}

	if ((operation == START || operation == STOP) && !device->IsStoppable()) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_OPERATION_DENIED_STOPPABLE, device->GetType());
	}

	if ((operation == INSERT || operation == EJECT) && !device->IsRemovable()) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_OPERATION_DENIED_REMOVABLE, device->GetType());
	}

	if ((operation == PROTECT || operation == UNPROTECT) && !device->IsProtectable()) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_OPERATION_DENIED_PROTECTABLE, device->GetType());
	}
	if ((operation == PROTECT || operation == UNPROTECT) && !device->IsReady()) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_OPERATION_DENIED_READY, device->GetType());
	}

	switch (operation) {
		case START:
			if (!dryRun) {
				LOGINFO("Start requested for %s ID %d, unit %d", device->GetType().c_str(), id, lun)

				if (!device->Start()) {
					LOGWARN("Starting %s ID %d, unit %d failed", device->GetType().c_str(), id, lun)
				}
			}
			break;

		case STOP:
			if (!dryRun) {
				LOGINFO("Stop requested for %s ID %d, unit %d", device->GetType().c_str(), id, lun)

				// STOP is idempotent
				device->Stop();
			}
			break;

		case INSERT:
			return Insert(context, pb_device, device, dryRun);

		case EJECT:
			if (!dryRun) {
				LOGINFO("Eject requested for %s ID %d, unit %d", device->GetType().c_str(), id, lun)

				if (!device->Eject(true)) {
					LOGWARN("Ejecting %s ID %d, unit %d failed", device->GetType().c_str(), id, lun)
				}
			}
			break;

		case PROTECT:
			if (!dryRun) {
				LOGINFO("Write protection requested for %s ID %d, unit %d", device->GetType().c_str(), id, lun)

				// PROTECT is idempotent
				device->SetProtected(true);
			}
			break;

		case UNPROTECT:
			if (!dryRun) {
				LOGINFO("Write unprotection requested for %s ID %d, unit %d", device->GetType().c_str(), id, lun)

				// UNPROTECT is idempotent
				device->SetProtected(false);
			}
			break;

		case ATTACH:
		case DETACH:
			// The non dry-run case has been handled before the switch
			assert(dryRun);
			break;

		case CHECK_AUTHENTICATION:
		case NO_OPERATION:
			// Do nothing, just log
			LOGTRACE("Received %s command", PbOperation_Name(operation).c_str())
			break;

		default:
			return ReturnLocalizedError(context, LocalizationKey::ERROR_OPERATION);
	}

	return true;
}

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

bool RascsiExecutor::Attach(const CommandContext& context, const PbDeviceDefinition& pb_device, bool dryRun)
{
	const int id = pb_device.id();
	const int lun = pb_device.unit();
	const PbDeviceType type = pb_device.type();

	if (controller_manager.GetDeviceByIdAndLun(id, lun) != nullptr) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_DUPLICATE_ID, to_string(id), to_string(lun));
	}

	if (lun >= ScsiController::LUN_MAX) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_INVALID_LUN, to_string(lun), to_string(ScsiController::LUN_MAX));
	}

	string filename = GetParam(pb_device, "file");

	PrimaryDevice *device = device_factory.CreateDevice(type, filename, id);
	if (device == nullptr) {
		if (type == UNDEFINED) {
			return ReturnLocalizedError(context, LocalizationKey::ERROR_MISSING_DEVICE_TYPE, filename);
		}
		else {
			return ReturnLocalizedError(context, LocalizationKey::ERROR_UNKNOWN_DEVICE_TYPE, PbDeviceType_Name(type));
		}
	}

	// If no filename was provided the medium is considered removed
	auto file_support = dynamic_cast<FileSupport *>(device);
	device->SetRemoved(file_support != nullptr ? filename.empty() : false);

	device->SetLun(lun);

	try {
		if (!pb_device.vendor().empty()) {
			device->SetVendor(pb_device.vendor());
		}
		if (!pb_device.product().empty()) {
			device->SetProduct(pb_device.product());
		}
		if (!pb_device.revision().empty()) {
			device->SetRevision(pb_device.revision());
		}
	}
	catch(const invalid_argument& e) { //NOSONAR This exception is handled properly
		return ReturnStatus(context, false, e.what());
	}

	if (pb_device.block_size()) {
		auto disk = dynamic_cast<Disk *>(device);
		if (disk != nullptr && disk->IsSectorSizeConfigurable()) {
			if (!disk->SetConfiguredSectorSize(device_factory, pb_device.block_size())) {
				device_factory.DeleteDevice(*device);

				return ReturnLocalizedError(context, LocalizationKey::ERROR_BLOCK_SIZE, to_string(pb_device.block_size()));
			}
		}
		else {
			device_factory.DeleteDevice(*device);

			return ReturnLocalizedError(context, LocalizationKey::ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, PbDeviceType_Name(type));
		}
	}

	// File check (type is HD, for removable media drives, CD and MO the medium (=file) may be inserted later
	if (file_support != nullptr && !device->IsRemovable() && filename.empty()) {
		device_factory.DeleteDevice(*device);

		return ReturnLocalizedError(context, LocalizationKey::ERROR_MISSING_FILENAME, PbDeviceType_Name(type));
	}

	Filepath filepath;
	if (file_support != nullptr && !filename.empty()) {
		filepath.SetPath(filename.c_str());
		string initial_filename = filepath.GetPath();

		int id;
		int unit;
		if (FileSupport::GetIdsForReservedFile(filepath, id, unit)) {
			device_factory.DeleteDevice(*device);

			return ReturnLocalizedError(context, LocalizationKey::ERROR_IMAGE_IN_USE, filename, to_string(id), to_string(unit));
		}

		try {
			try {
				file_support->Open(filepath);
			}
			catch(const file_not_found_exception&) {
				// If the file does not exist search for it in the default image folder
				filepath.SetPath(string(rascsi_image.GetDefaultImageFolder() + "/" + filename).c_str());

				if (FileSupport::GetIdsForReservedFile(filepath, id, unit)) {
					device_factory.DeleteDevice(*device);

					return ReturnLocalizedError(context, LocalizationKey::ERROR_IMAGE_IN_USE, filename, to_string(id), to_string(unit));
				}

				file_support->Open(filepath);
			}
		}
		catch(const io_exception& e) {
			device_factory.DeleteDevice(*device);

			return ReturnLocalizedError(context, LocalizationKey::ERROR_FILE_OPEN, initial_filename, e.get_msg());
		}

		file_support->ReserveFile(filepath, device->GetId(), device->GetLun());
	}

	// Only non read-only devices support protect/unprotect
	// This operation must not be executed before Open() because Open() overrides some settings.
	if (device->IsProtectable() && !device->IsReadOnly()) {
		device->SetProtected(pb_device.protected_());
	}

	// Stop the dry run here, before permanently modifying something
	if (dryRun) {
		device_factory.DeleteDevice(*device);

		return true;
	}

	unordered_map<string, string> params = { pb_device.params().begin(), pb_device.params().end() };
	if (!device->SupportsFile()) {
		params.erase("file");
	}
	if (!device->Init(params)) {
		device_factory.DeleteDevice(*device);

		return ReturnLocalizedError(context, LocalizationKey::ERROR_INITIALIZATION, PbDeviceType_Name(type), to_string(id), to_string(lun));
	}

	service.Lock();

	if (!controller_manager.CreateScsiController(bus, device)) {
		service.Unlock();

		return ReturnLocalizedError(context, LocalizationKey::ERROR_SCSI_CONTROLLER);
	}
	service.Unlock();

	string msg = "Attached ";
	if (device->IsReadOnly()) {
		msg += "read-only ";
	}
	else if (device->IsProtectable() && device->IsProtected()) {
		msg += "protected ";
	}
	msg += device->GetType() + " device, ID " + to_string(id) + ", unit " + to_string(lun);
	LOGINFO("%s", msg.c_str())

	return true;
}

bool RascsiExecutor::Insert(const CommandContext& context, const PbDeviceDefinition& pb_device, Device *device, bool dryRun)
{
	if (!device->IsRemoved()) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_EJECT_REQUIRED);
	}

	if (!pb_device.vendor().empty() || !pb_device.product().empty() || !pb_device.revision().empty()) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_DEVICE_NAME_UPDATE);
	}

	string filename = GetParam(pb_device, "file");
	if (filename.empty()) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_MISSING_FILENAME);
	}

	if (dryRun) {
		return true;
	}

	LOGINFO("Insert %sfile '%s' requested into %s ID %d, unit %d", pb_device.protected_() ? "protected " : "",
			filename.c_str(), device->GetType().c_str(), pb_device.id(), pb_device.unit())

	auto disk = dynamic_cast<Disk *>(device);

	if (pb_device.block_size()) {
		if (disk != nullptr&& disk->IsSectorSizeConfigurable()) {
			if (!disk->SetConfiguredSectorSize(device_factory, pb_device.block_size())) {
				return ReturnLocalizedError(context, LocalizationKey::ERROR_BLOCK_SIZE, to_string(pb_device.block_size()));
			}
		}
		else {
			return ReturnLocalizedError(context, LocalizationKey::ERROR_BLOCK_SIZE_NOT_CONFIGURABLE, device->GetType());
		}
	}

	int id;
	int lun;
	Filepath filepath;
	filepath.SetPath(filename.c_str());
	string initial_filename = filepath.GetPath();

	if (FileSupport::GetIdsForReservedFile(filepath, id, lun)) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_IMAGE_IN_USE, filename, to_string(id), to_string(lun));
	}

	auto file_support = dynamic_cast<FileSupport *>(device);
	try {
		try {
			file_support->Open(filepath);
		}
		catch(const file_not_found_exception&) {
			// If the file does not exist search for it in the default image folder
			filepath.SetPath((rascsi_image.GetDefaultImageFolder() + "/" + filename).c_str());

			if (FileSupport::GetIdsForReservedFile(filepath, id, lun)) {
				return ReturnLocalizedError(context, LocalizationKey::ERROR_IMAGE_IN_USE, filename, to_string(id), to_string(lun));
			}

			file_support->Open(filepath);
		}
	}
	catch(const io_exception& e) { //NOSONAR This exception is handled properly
		return ReturnLocalizedError(context, LocalizationKey::ERROR_FILE_OPEN, initial_filename, e.get_msg());
	}

	file_support->ReserveFile(filepath, device->GetId(), device->GetLun());

	// Only non read-only devices support protect/unprotect.
	// This operation must not be executed before Open() because Open() overrides some settings.
	if (device->IsProtectable() && !device->IsReadOnly()) {
		device->SetProtected(pb_device.protected_());
	}

	if (disk != nullptr) {
		disk->MediumChanged();
	}

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

bool RascsiExecutor::ShutDown(const CommandContext& context, const string& mode) {
	if (mode.empty()) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_SHUTDOWN_MODE_MISSING);
	}

	PbResult result;
	result.set_status(true);

	if (mode == "rascsi") {
		LOGINFO("RaSCSI shutdown requested");

		socket_connector.SerializeMessage(context.fd, result);

		return true;
	}

	// The root user has UID 0
	if (getuid()) {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_SHUTDOWN_PERMISSION);
	}

	if (mode == "system") {
		LOGINFO("System shutdown requested")

		socket_connector.SerializeMessage(context.fd, result);

		DetachAll();

		if (system("init 0") == -1) {
			LOGERROR("System shutdown failed: %s", strerror(errno))
		}
	}
	else if (mode == "reboot") {
		LOGINFO("System reboot requested")

		socket_connector.SerializeMessage(context.fd, result);

		DetachAll();

		if (system("init 6") == -1) {
			LOGERROR("System reboot failed: %s", strerror(errno))
		}
	}
	else {
		return ReturnLocalizedError(context, LocalizationKey::ERROR_SHUTDOWN_MODE_INVALID);
	}

	return false;
}
