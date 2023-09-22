//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_util.h"
#include "shared/protobuf_util.h"
#include "shared/piscsi_exceptions.h"
#include "controllers/controller_manager.h"
#include "controllers/scsi_controller.h"
#include "devices/device_factory.h"
#include "devices/primary_device.h"
#include "devices/disk.h"
#include "piscsi_image.h"
#include "localizer.h"
#include "command_context.h"
#include "piscsi_executor.h"
#include <spdlog/spdlog.h>
#include <sstream>

using namespace spdlog;
using namespace protobuf_util;
using namespace piscsi_util;

bool PiscsiExecutor::ProcessDeviceCmd(const CommandContext& context, const PbDeviceDefinition& pb_device, bool dryRun)
{
	spdlog::info((dryRun ? "Validating: " : "Executing: ") + PrintCommand(context.GetCommand(), pb_device));

	const int id = pb_device.id();
	const int lun = pb_device.unit();

	if (!ValidateIdAndLun(context, id, lun)) {
		return false;
	}

	const PbOperation operation = context.GetCommand().operation();

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
			spdlog::trace("Received " + PbOperation_Name(operation) + " command");
			break;

		default:
			return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION);
	}

	return true;
}

bool PiscsiExecutor::ProcessCmd(const CommandContext& context)
{
	const PbCommand& command = context.GetCommand();

	switch (command.operation()) {
		case DETACH_ALL:
			DetachAll();
			return context.ReturnSuccessStatus();

		case RESERVE_IDS: {
			const string ids = GetParam(command, "ids");
			if (const string error = SetReservedIds(ids); !error.empty()) {
				return context.ReturnErrorStatus(error);
			}

			return context.ReturnSuccessStatus();
		}

		case CREATE_IMAGE:
			return piscsi_image.CreateImage(context);

		case DELETE_IMAGE:
			return piscsi_image.DeleteImage(context);

		case RENAME_IMAGE:
			return piscsi_image.RenameImage(context);

		case COPY_IMAGE:
			return piscsi_image.CopyImage(context);

		case PROTECT_IMAGE:
		case UNPROTECT_IMAGE:
			return piscsi_image.SetImagePermissions(context);

		default:
			// This is a device-specific command handled below
			break;
	}

	// Remember the list of reserved files during the dry run
	const auto& reserved_files = StorageDevice::GetReservedFiles();
	const bool reserved = ranges::find_if_not(context.GetCommand().devices(), [&] (const auto& device)
			{ return ProcessDeviceCmd(context, device, true); }) != command.devices().end();
	StorageDevice::SetReservedFiles(reserved_files);
	if (reserved) {
		return false;
	}

	if (const string error = ValidateLunSetup(command); !error.empty()) {
		return context.ReturnErrorStatus(error);
	}

	if (ranges::find_if_not(command.devices(), [&] (const auto& device)
			{ return ProcessDeviceCmd(context, device, false); } ) != command.devices().end()) {
		return false;
	}

	// ATTACH and DETACH return the device list
	if (command.operation() == ATTACH || command.operation() == DETACH) {
		// A new command with an empty device list is required here in order to return data for all devices
		PbCommand cmd;
		PbResult result;
		piscsi_response.GetDevicesInfo(controller_manager.GetAllDevices(), result, cmd,
				piscsi_image.GetDefaultFolder());
		context.WriteResult(result);
		return true;
	}

	return context.ReturnSuccessStatus();
}

bool PiscsiExecutor::Start(PrimaryDevice& device, bool dryRun) const
{
	if (!dryRun) {
		spdlog::info("Start requested for " + device.GetIdentifier());

		if (!device.Start()) {
			spdlog::warn("Starting " + device.GetIdentifier() + " failed");
		}
	}

	return true;
}

bool PiscsiExecutor::Stop(PrimaryDevice& device, bool dryRun) const
{
	if (!dryRun) {
		spdlog::info("Stop requested for " + device.GetIdentifier());

		device.Stop();
	}

	return true;
}

bool PiscsiExecutor::Eject(PrimaryDevice& device, bool dryRun) const
{
	if (!dryRun) {
		spdlog::info("Eject requested for " + device.GetIdentifier());

		if (!device.Eject(true)) {
			spdlog::warn("Ejecting " + device.GetIdentifier() + " failed");
		}
	}

	return true;
}

bool PiscsiExecutor::Protect(PrimaryDevice& device, bool dryRun) const
{
	if (!dryRun) {
		spdlog::info("Write protection requested for " + device.GetIdentifier());

		device.SetProtected(true);
	}

	return true;
}

bool PiscsiExecutor::Unprotect(PrimaryDevice& device, bool dryRun) const
{
	if (!dryRun) {
		spdlog::info("Write unprotection requested for " + device.GetIdentifier());

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

	if (reserved_ids.contains(id)) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_RESERVED_ID, to_string(id));
	}

	const string filename = GetParam(pb_device, "file");

	auto device = CreateDevice(context, type, lun, filename);
	if (device == nullptr) {
		return false;
	}

	// If no filename was provided the medium is considered not inserted
	device->SetRemoved(device->SupportsFile() ? filename.empty() : false);

	if (!SetProductData(context, pb_device, *device)) {
		return false;
	}

	if (!SetSectorSize(context, device, pb_device.block_size())) {
		return false;
	}

	string full_path;
	const auto storage_device = dynamic_pointer_cast<StorageDevice>(device);
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
		return context.ReturnLocalizedError(LocalizationKey::ERROR_INITIALIZATION, device->GetIdentifier());
	}

	if (storage_device != nullptr) {
		storage_device->ReserveFile(full_path, id, lun);
	}

	if (!controller_manager.AttachToController(id, device)) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_SCSI_CONTROLLER);
	}

	string msg = "Attached ";
	if (device->IsReadOnly()) {
		msg += "read-only ";
	}
	else if (device->IsProtectable() && device->IsProtected()) {
		msg += "protected ";
	}
	msg += device->GetIdentifier();
	spdlog::info(msg);

	return true;
}

bool PiscsiExecutor::Insert(const CommandContext& context, const PbDeviceDefinition& pb_device,
		const shared_ptr<PrimaryDevice>& device, bool dryRun) const
{
	if (!device->SupportsFile()) {
		return false;
	}

	if (!device->IsRemoved()) {
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

	spdlog::info("Insert " + string(pb_device.protected_() ? "protected " : "") + "file '" + filename +
			"' requested into " + device->GetTypeString() + " " +
			to_string(pb_device.id()) + ":" + to_string(pb_device.unit()));

	auto storage_device = dynamic_pointer_cast<StorageDevice>(device);
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
		// Remember the device identifier before its ID becomes invalid by removing the device
		const string identifier = device->GetIdentifier();

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

		spdlog::info("Detached " + identifier);
	}

	return true;
}

void PiscsiExecutor::DetachAll()
{
	controller_manager.DeleteAllControllers();
	StorageDevice::UnreserveAll();

	spdlog::info("Detached all devices");
}

bool PiscsiExecutor::ShutDown(const CommandContext& context, const string& mode) {
	if (mode.empty()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_SHUTDOWN_MODE_MISSING);
	}

	PbResult result;
	result.set_status(true);

	// The PiSCSI shutdown mode is "rascsi" instead of "piscsi" for backwards compatibility
	if (mode == "rascsi") {
		spdlog::info("PiSCSI shutdown requested");

		context.WriteResult(result);

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
		spdlog::info("System shutdown requested");

		context.WriteResult(result);

		DetachAll();

		if (system("init 0") == -1) {
			LogErrno("System shutdown failed");
		}
	}
	else if (mode == "reboot") {
		spdlog::info("System reboot requested");

		context.WriteResult(result);

		DetachAll();

		if (system("init 6") == -1) {
			LogErrno("System reboot failed");
		}
	}
	else {
		assert(false);
	}

	return false;
}

string PiscsiExecutor::SetReservedIds(string_view ids)
{
	set<int> ids_to_reserve;
	stringstream ss(ids.data());
    string id;
    while (getline(ss, id, ',')) {
    	int res_id;
    	if (!GetAsUnsignedInt(id, res_id) || res_id > 7) {
    		return "Invalid ID " + id;
    	}

    	if (controller_manager.HasController(res_id)) {
    		return "ID " + id + " is currently in use";
    	}

    	ids_to_reserve.insert(res_id);
    }

    reserved_ids = { ids_to_reserve.begin(), ids_to_reserve.end() };

    if (!ids_to_reserve.empty()) {
    	spdlog::info("Reserved ID(s) set to " + Join(ids_to_reserve));
    }
    else {
    	spdlog::info("Cleared reserved ID(s)");
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
				to_string(id1) + ":" + to_string(lun1));
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
		storage_device.SetProtected(false);
	}
	else {
		storage_device.SetReadOnly(false);
		storage_device.SetProtectable(true);
	}

	try {
		storage_device.Open();
	}
	catch(const io_exception&) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_FILE_OPEN, effective_filename);
	}

	full_path = effective_filename;

	return true;
}

string PiscsiExecutor::PrintCommand(const PbCommand& command, const PbDeviceDefinition& pb_device) const
{
	const map<string, string, less<>> params = { command.params().begin(), command.params().end() };

	ostringstream s;
	s << "operation=" << PbOperation_Name(command.operation());

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

	return s.str();
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
	const auto& it = ranges::find_if_not(luns, [] (const auto& l) { return l.second & 0x01; } );
	return it == luns.end() ? "" : "LUN 0 is missing for device ID " + to_string((*it).first);
}

bool PiscsiExecutor::VerifyExistingIdAndLun(const CommandContext& context, int id, int lun) const
{
	if (!controller_manager.HasController(id)) {
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
		return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_STOPPABLE, PbOperation_Name(operation),
				device.GetTypeString());
	}

	if ((operation == INSERT || operation == EJECT) && !device.IsRemovable()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_REMOVABLE, PbOperation_Name(operation),
				device.GetTypeString());
	}

	if ((operation == PROTECT || operation == UNPROTECT) && !device.IsProtectable()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_PROTECTABLE, PbOperation_Name(operation),
				device.GetTypeString());
	}

	if ((operation == PROTECT || operation == UNPROTECT) && !device.IsReady()) {
		return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_READY, PbOperation_Name(operation),
				device.GetTypeString());
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
		return context.ReturnErrorStatus(e.what());
	}

	return true;
}
