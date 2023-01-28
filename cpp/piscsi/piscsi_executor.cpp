//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/log.h"
#include "shared/piscsi_util.h"
#include "shared/protobuf_util.h"
#include "shared/piscsi_exceptions.h"
#include "controllers/controller_manager.h"
#include "controllers/scsi_controller.h"
#include "devices/device_logger.h"
#include "devices/device_factory.h"
#include "devices/primary_device.h"
#include "devices/disk.h"
#include "piscsi_service.h"
#include "piscsi_image.h"
#include "localizer.h"
#include "command_context.h"
#include "piscsi_executor.h"
#include <sstream>

using namespace spdlog;
using namespace protobuf_util;
using namespace piscsi_util;

bool PiscsiExecutor::ProcessDeviceCmd(const CommandContext& context, const PbDeviceDefinition& pb_device,
		const PbCommand& command, bool dryRun)
{
	PrintCommand(command, pb_device, dryRun);

	const int id = pb_device.id();
	const int lun = pb_device.unit();

	if (!ValidateIdAndLun(context, id, lun)) {
		return false;
	}

	const PbOperation operation = command.operation();

	// For all commands except ATTACH the device and LUN must exist
	if (operation != ATTACH && !VerifyExistingIdAndLun(context, id, lun)) {
		return false;
	}

	auto device = controller_manager.GetDeviceByIdAndLun(id, lun);

	if (!ValidateOperationAgainstDevice(context, *device, operation)) {
		return false;
	}

	switch (operation) {
		case START:
			return Start(*device, dryRun);

		case STOP:
			return Stop(*device, dryRun);

		case ATTACH:
			return Attach(context, pb_device, dryRun);

		case DETACH:
			return Detach(context, device, dryRun);

		case INSERT:
			return Insert(context, pb_device, device, dryRun);

		case EJECT:
			return Eject(*device, dryRun);

		case PROTECT:
			return Protect(*device, dryRun);

		case UNPROTECT:
			return Unprotect(*device, dryRun);
			break;

		case CHECK_AUTHENTICATION:
		case NO_OPERATION:
			// Do nothing, just log
			LOGTRACE("Received %s command", PbOperation_Name(operation).c_str())
			break;

		default:
			return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION);
	}

	return true;
}

bool PiscsiExecutor::ProcessCmd(const CommandContext& context, const PbCommand& command)
{
	switch (command.operation()) {
		case DETACH_ALL:
			DetachAll();
			return context.ReturnStatus();

		case RESERVE_IDS: {
			const string ids = GetParam(command, "ids");
			if (const string error = SetReservedIds(ids); !error.empty()) {
				return context.ReturnStatus(false, error);
			}

			return context.ReturnStatus();
		}

		case CREATE_IMAGE:
			return piscsi_image.CreateImage(context, command);

		case DELETE_IMAGE:
			return piscsi_image.DeleteImage(context, command);

		case RENAME_IMAGE:
			return piscsi_image.RenameImage(context, command);

		case COPY_IMAGE:
			return piscsi_image.CopyImage(context, command);

		case PROTECT_IMAGE:
		case UNPROTECT_IMAGE:
			return piscsi_image.SetImagePermissions(context, command);

		default:
			// This is a device-specific command handled below
			break;
	}

	// Remember the list of reserved files, than run the dry run
	const auto& reserved_files = StorageDevice::GetReservedFiles();
	for (const auto& device : command.devices()) {
		if (!ProcessDeviceCmd(context, device, command, true)) {
			// Dry run failed, restore the file list
			StorageDevice::SetReservedFiles(reserved_files);
			return false;
		}
	}

	// Restore the list of reserved files before proceeding
	StorageDevice::SetReservedFiles(reserved_files);

	if (const string result = ValidateLunSetup(command); !result.empty()) {
		return context.ReturnStatus(false, result);
	}

	for (const auto& device : command.devices()) {
		if (!ProcessDeviceCmd(context, device, command, false)) {
			return false;
		}
	}

	// ATTACH and DETACH return the device list
	if (context.IsValid() && (command.operation() == ATTACH || command.operation() == DETACH)) {
		// A new command with an empty device list is required here in order to return data for all devices
		PbCommand cmd;
		PbResult result;
		piscsi_response.GetDevicesInfo(controller_manager.GetAllDevices(), result, cmd,
				piscsi_image.GetDefaultFolder());
		serializer.SerializeMessage(context.GetFd(), result);
		return true;
	}

	return context.ReturnStatus();
}

bool PiscsiExecutor::SetLogLevel(const string& log_level) const
{
	int id = -1;
	int lun = -1;
	string level = log_level;

	if (size_t separator_pos = log_level.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
		level = log_level.substr(0, separator_pos);

		const string l = log_level.substr(separator_pos + 1);
		separator_pos = l.find(COMPONENT_SEPARATOR);
		if (separator_pos != string::npos) {
			const string error = ProcessId(l, ScsiController::LUN_MAX, id, lun);
			if (!error.empty()) {
				LOGWARN("Invalid device ID/LUN specifier '%s'", l.c_str())
				return false;
			}
		}
		else if (!GetAsUnsignedInt(l, id)) {
			LOGWARN("Invalid device ID specifier '%s'", l.c_str())
			return false;
		}
	}

	if (const auto& it = log_level_mapping.find(level); it != log_level_mapping.end()) {
		set_level(it->second);
	}
	else {
		LOGWARN("Invalid log level '%s'", log_level.c_str())
		return false;
	}

	DeviceLogger::SetLogIdAndLun(id, lun);

	if (id != -1) {
		if (lun == -1) {
			LOGINFO("Set log level for device ID %d to '%s'", id, level.c_str())
		}
		else {
			LOGINFO("Set log level for device ID %d, LUN %d to '%s'", id, lun, level.c_str())
		}
	}
	else {
		LOGINFO("Set log level to '%s'", level.c_str())
	}

	return true;
}

bool PiscsiExecutor::Start(PrimaryDevice& device, bool dryRun) const
{
	if (!dryRun) {
		LOGINFO("Start requested for %s ID %d, unit %d", device.GetTypeString(), device.GetId(), device.GetLun())

		if (!device.Start()) {
			LOGWARN("Starting %s ID %d, unit %d failed", device.GetTypeString(), device.GetId(), device.GetLun())
		}
	}

	return true;
}

bool PiscsiExecutor::Stop(PrimaryDevice& device, bool dryRun) const
{
	if (!dryRun) {
		LOGINFO("Stop requested for %s ID %d, unit %d", device.GetTypeString(), device.GetId(), device.GetLun())

		device.Stop();
	}

	return true;
}

bool PiscsiExecutor::Eject(PrimaryDevice& device, bool dryRun) const
{
	if (!dryRun) {
		LOGINFO("Eject requested for %s ID %d, unit %d", device.GetTypeString(), device.GetId(), device.GetLun())

		if (!device.Eject(true)) {
			LOGWARN("Ejecting %s ID %d, unit %d failed", device.GetTypeString(), device.GetId(), device.GetLun())
		}
	}

	return true;
}

bool PiscsiExecutor::Protect(PrimaryDevice& device, bool dryRun) const
{
	if (!dryRun) {
		LOGINFO("Write protection requested for %s ID %d, unit %d", device.GetTypeString(), device.GetId(),
				device.GetLun())

		device.SetProtected(true);
	}

	return true;
}

bool PiscsiExecutor::Unprotect(PrimaryDevice& device, bool dryRun) const
{
	if (!dryRun) {
		LOGINFO("Write unprotection requested for %s ID %d, unit %d", device.GetTypeString(), device.GetId(),
				device.GetLun())

		device.SetProtected(false);
	}

	return true;
}

bool PiscsiExecutor::Attach(const CommandContext& context, const PbDeviceDefinition& pb_device, bool dryRun)
{
	const int id = pb_device.id();
	const int lun = pb_device.unit();
	const PbDeviceType type = pb_device.type();

	if (lun >= ScsiController::LUN_MAX) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_INVALID_LUN, to_string(lun), to_string(ScsiController::LUN_MAX));
	}

	if (controller_manager.GetDeviceByIdAndLun(id, lun) != nullptr) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_DUPLICATE_ID, to_string(id), to_string(lun));
	}

	if (reserved_ids.find(id) != reserved_ids.end()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_RESERVED_ID, to_string(id));
	}

	const string filename = GetParam(pb_device, "file");

	auto device = CreateDevice(context, type, lun, filename);
	if (device == nullptr) {
		return false;
	}

	// If no filename was provided the medium is considered not inserted
	auto storage_device = dynamic_pointer_cast<StorageDevice>(device);
	device->SetRemoved(storage_device != nullptr ? filename.empty() : false);

	if (!SetProductData(context, pb_device, *device)) {
		return false;
	}

	if (!SetSectorSize(context, device, pb_device.block_size())) {
		return false;
	}

	string full_path;
	if (device->SupportsFile()) {
		// Only with removable media drives, CD and MO the medium (=file) may be inserted later
		if (!device->IsRemovable() && filename.empty()) {
			return context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_FILENAME, PbDeviceType_Name(type));
		}

		if (!ValidateImageFile(context, *storage_device, filename, full_path)) {
			return false;
		}
	}

	// Only non read-only devices support protect/unprotect
	// This operation must not be executed before Open() because Open() overrides some settings.
	if (device->IsProtectable() && !device->IsReadOnly()) {
		device->SetProtected(pb_device.protected_());
	}

	// Stop the dry run here, before actually attaching
	if (dryRun) {
		return true;
	}

	unordered_map<string, string> params = { pb_device.params().begin(), pb_device.params().end() };
	if (!device->SupportsFile()) {
		// Clients like scsictl might have sent both "file" and "interfaces"
		params.erase("file");
	}

	if (!device->Init(params)) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_INITIALIZATION, PbDeviceType_Name(device->GetType()),
				to_string(id), to_string(lun));
	}

	if (storage_device != nullptr) {
		storage_device->ReserveFile(full_path, id, lun);
	}

	if (!controller_manager.AttachToScsiController(id, device)) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_SCSI_CONTROLLER);
	}

	string msg = "Attached ";
	if (device->IsReadOnly()) {
		msg += "read-only ";
	}
	else if (device->IsProtectable() && device->IsProtected()) {
		msg += "protected ";
	}
	msg += string(device->GetTypeString()) + " device, ID " + to_string(id) + ", unit " + to_string(lun);
	LOGINFO("%s", msg.c_str())

	return true;
}

bool PiscsiExecutor::Insert(const CommandContext& context, const PbDeviceDefinition& pb_device,
		const shared_ptr<PrimaryDevice>& device, bool dryRun) const
{
	auto storage_device = dynamic_pointer_cast<StorageDevice>(device);
	if (storage_device == nullptr) {
		return false;
	}

	if (!storage_device->IsRemoved()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_EJECT_REQUIRED);
	}

	if (!pb_device.vendor().empty() || !pb_device.product().empty() || !pb_device.revision().empty()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_DEVICE_NAME_UPDATE);
	}

	const string filename = GetParam(pb_device, "file");
	if (filename.empty()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_FILENAME);
	}

	// Stop the dry run here, before modifying the device
	if (dryRun) {
		return true;
	}

	LOGINFO("Insert %sfile '%s' requested into %s ID %d, unit %d", pb_device.protected_() ? "protected " : "",
			filename.c_str(), storage_device->GetTypeString(), pb_device.id(), pb_device.unit())

	if (!SetSectorSize(context, storage_device, pb_device.block_size())) {
		return false;
	}

	string full_path;
	if (!ValidateImageFile(context, *storage_device, filename, full_path)) {
		return false;
	}

	storage_device->SetProtected(pb_device.protected_());
	storage_device->ReserveFile(full_path, storage_device->GetId(), storage_device->GetLun());
	storage_device->SetMediumChanged(true);

	return true;
}

bool PiscsiExecutor::Detach(const CommandContext& context, const shared_ptr<PrimaryDevice>& device, bool dryRun) const
{
	auto controller = controller_manager.FindController(device->GetId());
	if (controller == nullptr) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_DETACH);
	}

	// LUN 0 can only be detached if there is no other LUN anymore
	if (!device->GetLun() && controller->GetLunCount() > 1) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_LUN0);
	}

	if (!dryRun) {
		// Remember the ID before it gets invalid when removing the device
		const int id = device->GetId();

		if (!controller->RemoveDevice(device)) {
			return context.ReturnLocalizedError(LocalizationKey::ERROR_DETACH);
		}

		// If no LUN is left also delete the controller
		if (!controller->GetLunCount() && !controller_manager.DeleteController(controller)) {
			return context.ReturnLocalizedError(LocalizationKey::ERROR_DETACH);
		}

		if (auto storage_device = dynamic_pointer_cast<StorageDevice>(device); storage_device != nullptr) {
			storage_device->UnreserveFile();
		}

		LOGINFO("%s", ("Detached " + string(device->GetTypeString()) + " device with ID " + to_string(id)
				+ ", unit " + to_string(device->GetLun())).c_str())
	}

	return true;
}

void PiscsiExecutor::DetachAll()
{
	controller_manager.DeleteAllControllers();
	StorageDevice::UnreserveAll();

	LOGINFO("Detached all devices")
}

bool PiscsiExecutor::ShutDown(const CommandContext& context, const string& mode) {
	if (mode.empty()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_SHUTDOWN_MODE_MISSING);
	}

	PbResult result;
	result.set_status(true);

	// The PiSCSI shutdown mode is "rascsi" instead of "piscsi" for backwards compatibility
	if (mode == "rascsi") {
		LOGINFO("PiSCSI shutdown requested")

		serializer.SerializeMessage(context.GetFd(), result);

		return true;
	}

	if (mode != "system" && mode != "reboot") {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_SHUTDOWN_MODE_INVALID, mode);
	}

	// The root user has UID 0
	if (getuid()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_SHUTDOWN_PERMISSION);
	}

	if (mode == "system") {
		LOGINFO("System shutdown requested")

		serializer.SerializeMessage(context.GetFd(), result);

		DetachAll();

		if (system("init 0") == -1) {
			LOGERROR("System shutdown failed: %s", strerror(errno))
		}
	}
	else if (mode == "reboot") {
		LOGINFO("System reboot requested")

		serializer.SerializeMessage(context.GetFd(), result);

		DetachAll();

		if (system("init 6") == -1) {
			LOGERROR("System reboot failed: %s", strerror(errno))
		}
	}
	else {
		assert(false);
	}

	return false;
}

string PiscsiExecutor::SetReservedIds(string_view ids)
{
	list<string> ids_to_reserve;
	stringstream ss(ids.data());
    string id;
    while (getline(ss, id, ',')) {
    	if (!id.empty()) {
    		ids_to_reserve.push_back(id);
    	}
    }

    set<int> reserved;
    for (const string& id_to_reserve : ids_to_reserve) {
    	int res_id;
 		if (!GetAsUnsignedInt(id_to_reserve, res_id) || res_id > 7) {
 			return "Invalid ID " + id_to_reserve;
 		}

 		if (controller_manager.FindController(res_id) != nullptr) {
 			return "ID " + id_to_reserve + " is currently in use";
 		}

 		reserved.insert(res_id);
    }

    reserved_ids = { reserved.begin(), reserved.end() };

    if (!reserved_ids.empty()) {
    	string s;
    	bool isFirst = true;
    	for (const auto& reserved_id : reserved) {
    		if (!isFirst) {
    			s += ", ";
    		}
    		isFirst = false;
    		s += to_string(reserved_id);
    	}

    	LOGINFO("Reserved ID(s) set to %s", s.c_str())
    }
    else {
    	LOGINFO("Cleared reserved ID(s)")
    }

	return "";
}

bool PiscsiExecutor::ValidateImageFile(const CommandContext& context, StorageDevice& storage_device,
		const string& filename, string& full_path) const
{
	if (filename.empty()) {
		return true;
	}

	if (const auto [id1, lun1] = StorageDevice::GetIdsForReservedFile(filename); id1 != -1 || lun1 != -1) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_IMAGE_IN_USE, filename,
				to_string(id1), to_string(lun1));
	}

	string effective_filename = filename;

	if (!StorageDevice::FileExists(filename)) {
		// If the file does not exist search for it in the default image folder
		effective_filename = piscsi_image.GetDefaultFolder() + "/" + filename;

		if (const auto [id2, lun2] = StorageDevice::GetIdsForReservedFile(effective_filename); id2 != -1 || lun2 != -1) {
			return context.ReturnLocalizedError(LocalizationKey::ERROR_IMAGE_IN_USE, filename,
					to_string(id2), to_string(lun2));
		}

		if (!StorageDevice::FileExists(effective_filename)) {
			return context.ReturnLocalizedError(LocalizationKey::ERROR_FILE_OPEN, effective_filename);
		}
	}

	storage_device.SetFilename(effective_filename);

	if (storage_device.IsReadOnlyFile()) {
		// Permanently write-protected
		storage_device.SetReadOnly(true);
		storage_device.SetProtectable(false);
	}
	else {
		storage_device.SetReadOnly(false);
		storage_device.SetProtectable(true);
	}

	try {
		storage_device.Open();
	}
	catch(const io_exception& e) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_FILE_OPEN, effective_filename);
	}

	full_path = effective_filename;

	return true;
}

void PiscsiExecutor::PrintCommand(const PbCommand& command, const PbDeviceDefinition& pb_device, bool dryRun) const
{
	const map<string, string, less<>> params = { command.params().begin(), command.params().end() };

	ostringstream s;
	s << (dryRun ? "Validating" : "Executing");
	s << ": operation=" << PbOperation_Name(command.operation());

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

	s << ", device id=" << pb_device.id() << ", lun=" << pb_device.unit() << ", type="
			<< PbDeviceType_Name(pb_device.type());

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
}

string PiscsiExecutor::ValidateLunSetup(const PbCommand& command) const
{
	// Mapping of available LUNs (bit vector) to devices
	unordered_map<uint32_t, uint32_t> luns;

	// Collect LUN bit vectors of new devices
	for (const auto& device : command.devices()) {
		luns[device.id()] |= 1 << device.unit();
	}

	// Collect LUN bit vectors of existing devices
	for (const auto& device : controller_manager.GetAllDevices()) {
		luns[device->GetId()] |= 1 << device->GetLun();
	}

	// LUN 0 must exist for all devices
	for (const auto& [id, lun]: luns) {
		if (!(lun & 0x01)) {
			return "LUN 0 is missing for device ID " + to_string(id);
		}
	}

	return "";
}

bool PiscsiExecutor::VerifyExistingIdAndLun(const CommandContext& context, int id, int lun) const
{
	if (controller_manager.FindController(id) == nullptr) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_NON_EXISTING_DEVICE, to_string(id));
	}

	if (controller_manager.GetDeviceByIdAndLun(id, lun) == nullptr) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_NON_EXISTING_UNIT, to_string(id), to_string(lun));
	}

	return true;
}

shared_ptr<PrimaryDevice> PiscsiExecutor::CreateDevice(const CommandContext& context, const PbDeviceType type,
		int lun, const string& filename) const
{
	auto device = device_factory.CreateDevice(type, lun, filename);
	if (device == nullptr) {
		if (type == UNDEFINED) {
			context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_DEVICE_TYPE, filename);
		}
		else {
			context.ReturnLocalizedError(LocalizationKey::ERROR_UNKNOWN_DEVICE_TYPE, PbDeviceType_Name(type));
		}
	}

	return device;
}

bool PiscsiExecutor::SetSectorSize(const CommandContext& context, shared_ptr<PrimaryDevice> device, int block_size) const
{
	if (block_size) {
		auto disk = dynamic_pointer_cast<Disk>(device);
		if (disk != nullptr && disk->IsSectorSizeConfigurable()) {
			if (!disk->SetConfiguredSectorSize(device_factory, block_size)) {
				return context.ReturnLocalizedError(LocalizationKey::ERROR_BLOCK_SIZE, to_string(block_size));
			}
		}
		else {
			return context.ReturnLocalizedError(LocalizationKey::ERROR_BLOCK_SIZE_NOT_CONFIGURABLE,
					device->GetTypeString());
		}
	}

	return true;
}

bool PiscsiExecutor::ValidateOperationAgainstDevice(const CommandContext& context, const PrimaryDevice& device,
		const PbOperation& operation)
{
	if ((operation == START || operation == STOP) && !device.IsStoppable()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_STOPPABLE, device.GetTypeString());
	}

	if ((operation == INSERT || operation == EJECT) && !device.IsRemovable()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_REMOVABLE, device.GetTypeString());
	}

	if ((operation == PROTECT || operation == UNPROTECT) && !device.IsProtectable()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_PROTECTABLE, device.GetTypeString());
	}

	if ((operation == PROTECT || operation == UNPROTECT) && !device.IsReady()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_READY, device.GetTypeString());
	}

	return true;
}

bool PiscsiExecutor::ValidateIdAndLun(const CommandContext& context, int id, int lun)
{
	// Validate the device ID and LUN
	if (id < 0) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_DEVICE_ID);
	}
	if (id >= ControllerManager::DEVICE_MAX) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_INVALID_ID, to_string(id), to_string(ControllerManager::DEVICE_MAX - 1));
	}
	if (lun < 0 || lun >= ScsiController::LUN_MAX) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_INVALID_LUN, to_string(lun), to_string(ScsiController::LUN_MAX - 1));
	}

	return true;
}

bool PiscsiExecutor::SetProductData(const CommandContext& context, const PbDeviceDefinition& pb_device,
		PrimaryDevice& device)
{
	try {
		if (!pb_device.vendor().empty()) {
			device.SetVendor(pb_device.vendor());
		}
		if (!pb_device.product().empty()) {
			device.SetProduct(pb_device.product());
		}
		if (!pb_device.revision().empty()) {
			device.SetRevision(pb_device.revision());
		}
	}
	catch(const invalid_argument& e) {
		return context.ReturnStatus(false, e.what());
	}

	return true;
}
