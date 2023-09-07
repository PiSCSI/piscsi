//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/protobuf_util.h"
#include "shared/network_util.h"
#include "shared/piscsi_util.h"
#include "shared/piscsi_version.h"
#include "devices/disk.h"
#include "piscsi_response.h"
#include <spdlog/spdlog.h>
#include <filesystem>

using namespace std;
using namespace filesystem;
using namespace piscsi_interface;
using namespace piscsi_util;
using namespace network_util;
using namespace protobuf_util;

unique_ptr<PbDeviceProperties> PiscsiResponse::GetDeviceProperties(const Device& device) const
{
	auto properties = make_unique<PbDeviceProperties>();

	// Currently there is only a SCSI controller, i.e. there can always be 32 LUNs
	properties->set_luns(32);
	properties->set_read_only(device.IsReadOnly());
	properties->set_protectable(device.IsProtectable());
	properties->set_stoppable(device.IsStoppable());
	properties->set_removable(device.IsRemovable());
	properties->set_lockable(device.IsLockable());
	properties->set_supports_file(device.SupportsFile());
	properties->set_supports_params(device.SupportsParams());

	if (device.SupportsParams()) {
		for (const auto& [key, value] : device_factory.GetDefaultParams(device.GetType())) {
			auto& map = *properties->mutable_default_params();
			map[key] = value;
		}
	}

	for (const auto& block_size : device_factory.GetSectorSizes(device.GetType())) {
		properties->add_block_sizes(block_size);
	}

	return properties;
}

void PiscsiResponse::GetDeviceTypeProperties(PbDeviceTypesInfo& device_types_info, PbDeviceType type) const
{
	auto type_properties = device_types_info.add_properties();
	type_properties->set_type(type);
	const auto device = device_factory.CreateDevice(type, 0, "");
	type_properties->set_allocated_properties(GetDeviceProperties(*device).release());
} //NOSONAR The allocated memory is managed by protobuf

void PiscsiResponse::GetAllDeviceTypeProperties(PbDeviceTypesInfo& device_types_info) const
{
	// Start with 2 instead of 1. 1 was the removed SASI drive type.
	int ordinal = 2;
	while (PbDeviceType_IsValid(ordinal)) {
		PbDeviceType type = UNDEFINED;
		PbDeviceType_Parse(PbDeviceType_Name((PbDeviceType)ordinal), &type);
		GetDeviceTypeProperties(device_types_info, type);
		ordinal++;
	}
}

void PiscsiResponse::GetDevice(const Device& device, PbDevice& pb_device, const string& default_folder) const
{
	pb_device.set_id(device.GetId());
	pb_device.set_unit(device.GetLun());
	pb_device.set_vendor(device.GetVendor());
	pb_device.set_product(device.GetProduct());
	pb_device.set_revision(device.GetRevision());
	pb_device.set_type(device.GetType());

    pb_device.set_allocated_properties(GetDeviceProperties(device).release());

    auto status = make_unique<PbDeviceStatus>();
	pb_device.set_allocated_status(status.release());
	status->set_protected_(device.IsProtected());
	status->set_stopped(device.IsStopped());
	status->set_removed(device.IsRemoved());
	status->set_locked(device.IsLocked());

	if (device.SupportsParams()) { //NOSONAR The allocated memory is managed by protobuf
		for (const auto& [key, value] : device.GetParams()) {
			SetParam(pb_device, key, value);
		}
	}

    if (const auto disk = dynamic_cast<const Disk*>(&device); disk) {
    	pb_device.set_block_size(device.IsRemoved()? 0 : disk->GetSectorSizeInBytes());
    	pb_device.set_block_count(device.IsRemoved() ? 0: disk->GetBlockCount());
    }

    const auto storage_device = dynamic_cast<const StorageDevice *>(&device);
	if (storage_device != nullptr) {
		auto image_file = make_unique<PbImageFile>().release();
		GetImageFile(*image_file, default_folder, device.IsReady() ? storage_device->GetFilename() : "");
		pb_device.set_allocated_file(image_file);
	}
} //NOSONAR The allocated memory is managed by protobuf

bool PiscsiResponse::GetImageFile(PbImageFile& image_file, const string& default_folder, const string& filename) const
{
	if (!filename.empty()) {
		image_file.set_name(filename);
		image_file.set_type(device_factory.GetTypeForFile(filename));

		const path p(filename[0] == '/' ? filename : default_folder + "/" + filename);

		image_file.set_read_only(access(p.c_str(), W_OK));

		error_code error;
		if (is_regular_file(p, error) || (is_symlink(p, error) && !is_block_file(p, error))) {
			image_file.set_size(file_size(p));
			return true;
		}
	}

	return false;
}

void PiscsiResponse::GetAvailableImages(PbImageFilesInfo& image_files_info, const string& default_folder,
		const string& folder_pattern, const string& file_pattern, int scan_depth) const
{
	const path default_path(default_folder);
	if (!is_directory(default_path)) {
		return;
	}

	string folder_pattern_lower = folder_pattern;
	ranges::transform(folder_pattern_lower, folder_pattern_lower.begin(), ::tolower);

	string file_pattern_lower = file_pattern;
	ranges::transform(file_pattern_lower, file_pattern_lower.begin(), ::tolower);

	for (auto it = recursive_directory_iterator(default_path, directory_options::follow_directory_symlink);
		it != recursive_directory_iterator(); it++) {
		if (it.depth() > scan_depth) {
			it.disable_recursion_pending();
			continue;
		}

		const string parent = it->path().parent_path().string();

		const string folder = parent.size() > default_folder.size() ? parent.substr(default_folder.size() + 1) : "";

		if (!FilterMatches(folder, folder_pattern_lower) || !FilterMatches(it->path().filename().string(), file_pattern_lower)) {
			continue;
		}

		if (!ValidateImageFile(it->path())) {
			continue;
		}

		const string filename = folder.empty() ?
				it->path().filename().string() : folder + "/" + it->path().filename().string();
		if (auto image_file = make_unique<PbImageFile>(); GetImageFile(*image_file.get(), default_folder, filename)) {
			GetImageFile(*image_files_info.add_image_files(), default_folder, filename);
		}
	}
}

unique_ptr<PbImageFilesInfo> PiscsiResponse::GetAvailableImages(PbResult& result, const string& default_folder,
		const string& folder_pattern, const string& file_pattern, int scan_depth) const
{
	auto image_files_info = make_unique<PbImageFilesInfo>();

	image_files_info->set_default_image_folder(default_folder);
	image_files_info->set_depth(scan_depth);

	GetAvailableImages(*image_files_info, default_folder, folder_pattern, file_pattern, scan_depth);

	result.set_status(true);

	return image_files_info;
}

void PiscsiResponse::GetAvailableImages(PbResult& result, PbServerInfo& server_info, const string& default_folder,
		const string& folder_pattern, const string& file_pattern, int scan_depth) const
{
	auto image_files_info = GetAvailableImages(result, default_folder, folder_pattern, file_pattern, scan_depth);
	image_files_info->set_default_image_folder(default_folder);
	server_info.set_allocated_image_files_info(image_files_info.release());

	result.set_status(true); //NOSONAR The allocated memory is managed by protobuf
}

unique_ptr<PbReservedIdsInfo> PiscsiResponse::GetReservedIds(PbResult& result, const unordered_set<int>& ids) const
{
	auto reserved_ids_info = make_unique<PbReservedIdsInfo>();
	for (const int id : ids) {
		reserved_ids_info->add_ids(id);
	}

	result.set_status(true);

	return reserved_ids_info;
}

void PiscsiResponse::GetDevices(const unordered_set<shared_ptr<PrimaryDevice>>& devices, PbServerInfo& server_info,
		const string& default_folder) const
{
	for (const auto& device : devices) {
		PbDevice *pb_device = server_info.mutable_devices_info()->add_devices();
		GetDevice(*device, *pb_device, default_folder);
	}
}

void PiscsiResponse::GetDevicesInfo(const unordered_set<shared_ptr<PrimaryDevice>>& devices, PbResult& result,
		const PbCommand& command, const string& default_folder) const
{
	set<id_set> id_sets;

	// If no device list was provided in the command get information on all devices
	if (!command.devices_size()) {
		for (const auto& device : devices) {
			id_sets.insert({ device->GetId(), device->GetLun() });
		}
	}
	// Otherwise get information on the devices provided in the command
	else {
		id_sets = MatchDevices(devices, result, command);
		if (id_sets.empty()) {
			return;
		}
	}

	auto devices_info = make_unique<PbDevicesInfo>();

	for (const auto& [id, lun] : id_sets) {
		for (const auto& d : devices) {
			if (d->GetId() == id && d->GetLun() == lun) {
				GetDevice(*d, *devices_info->add_devices(), default_folder);
				break;
			}
		}
	}

	result.set_allocated_devices_info(devices_info.release());
	result.set_status(true);
}

unique_ptr<PbDeviceTypesInfo> PiscsiResponse::GetDeviceTypesInfo(PbResult& result) const
{
	auto device_types_info = make_unique<PbDeviceTypesInfo>();

	GetAllDeviceTypeProperties(*device_types_info);

	result.set_status(true);

	return device_types_info;
}

unique_ptr<PbServerInfo> PiscsiResponse::GetServerInfo(const unordered_set<shared_ptr<PrimaryDevice>>& devices,
		PbResult& result, const unordered_set<int>& reserved_ids, const string& current_log_level,
		const string& default_folder, const string& folder_pattern, const string& file_pattern, int scan_depth) const
{
	auto server_info = make_unique<PbServerInfo>();

	server_info->set_allocated_version_info(GetVersionInfo(result).release());
	server_info->set_allocated_log_level_info(GetLogLevelInfo(result, current_log_level).release()); //NOSONAR The allocated memory is managed by protobuf
	GetAllDeviceTypeProperties(*server_info->mutable_device_types_info()); //NOSONAR The allocated memory is managed by protobuf
	GetAvailableImages(result, *server_info, default_folder, folder_pattern, file_pattern, scan_depth);
	server_info->set_allocated_network_interfaces_info(GetNetworkInterfacesInfo(result).release());
	server_info->set_allocated_mapping_info(GetMappingInfo(result).release()); //NOSONAR The allocated memory is managed by protobuf
	GetDevices(devices, *server_info, default_folder); //NOSONAR The allocated memory is managed by protobuf
	server_info->set_allocated_reserved_ids_info(GetReservedIds(result, reserved_ids).release());
	server_info->set_allocated_operation_info(GetOperationInfo(result, scan_depth).release()); //NOSONAR The allocated memory is managed by protobuf

	result.set_status(true);

	return server_info;
}

unique_ptr<PbVersionInfo> PiscsiResponse::GetVersionInfo(PbResult& result) const
{
	auto version_info = make_unique<PbVersionInfo>();

	version_info->set_major_version(piscsi_major_version);
	version_info->set_minor_version(piscsi_minor_version);
	version_info->set_patch_version(piscsi_patch_version);

	result.set_status(true);

	return version_info;
}

unique_ptr<PbLogLevelInfo> PiscsiResponse::GetLogLevelInfo(PbResult& result, const string& current_log_level) const
{
	auto log_level_info = make_unique<PbLogLevelInfo>();

	for (const auto& log_level : log_levels) {
		log_level_info->add_log_levels(log_level);
	}

	log_level_info->set_current_log_level(current_log_level);

	result.set_status(true);

	return log_level_info;
}

unique_ptr<PbNetworkInterfacesInfo> PiscsiResponse::GetNetworkInterfacesInfo(PbResult& result) const
{
	auto network_interfaces_info = make_unique<PbNetworkInterfacesInfo>();

	for (const auto& network_interface : GetNetworkInterfaces()) {
		network_interfaces_info->add_name(network_interface);
	}

	result.set_status(true);

	return network_interfaces_info;
}

unique_ptr<PbMappingInfo> PiscsiResponse::GetMappingInfo(PbResult& result) const
{
	auto mapping_info = make_unique<PbMappingInfo>();

	for (const auto& [name, type] : device_factory.GetExtensionMapping()) {
		(*mapping_info->mutable_mapping())[name] = type;
	}

	result.set_status(true);

	return mapping_info;
}

unique_ptr<PbOperationInfo> PiscsiResponse::GetOperationInfo(PbResult& result, int depth) const
{
	auto operation_info = make_unique<PbOperationInfo>();

	auto operation = CreateOperation(*operation_info, ATTACH, "Attach device, device-specific parameters are required");
	AddOperationParameter(*operation, "name", "Image file name in case of a mass storage device").release();
	AddOperationParameter(*operation, "interface", "Comma-separated prioritized network interface list").release();
	AddOperationParameter(*operation, "inet", "IP address and netmask of the network bridge").release();
	AddOperationParameter(*operation, "cmd", "Print command for the printer device").release();
	operation.release();

	CreateOperation(*operation_info, DETACH, "Detach device, device-specific parameters are required").release();

	CreateOperation(*operation_info, DETACH_ALL, "Detach all devices").release();

	CreateOperation(*operation_info, START, "Start device, device-specific parameters are required").release();

	CreateOperation(*operation_info, STOP, "Stop device, device-specific parameters are required").release();

	operation = CreateOperation(*operation_info, INSERT, "Insert medium, device-specific parameters are required");
	AddOperationParameter(*operation, "file", "Image file name", "", true).release();
	operation.release();

	CreateOperation(*operation_info, EJECT, "Eject medium, device-specific parameters are required").release();

	CreateOperation(*operation_info, PROTECT, "Protect medium, device-specific parameters are required").release();

	CreateOperation(*operation_info, UNPROTECT, "Unprotect medium, device-specific parameters are required").release();

	operation = CreateOperation(*operation_info, SERVER_INFO, "Get piscsi server information");
	if (depth) {
		AddOperationParameter(*operation, "folder_pattern", "Pattern for filtering image folder names").release();
	}
	AddOperationParameter(*operation, "file_pattern", "Pattern for filtering image file names").release();
	operation.release();

	CreateOperation(*operation_info, VERSION_INFO, "Get piscsi server version").release();

	CreateOperation(*operation_info, DEVICES_INFO, "Get information on attached devices").release();

	CreateOperation(*operation_info, DEVICE_TYPES_INFO, "Get device properties by device type").release();

	operation = CreateOperation(*operation_info, DEFAULT_IMAGE_FILES_INFO, "Get information on available image files");
	if (depth) {
		AddOperationParameter(*operation, "folder_pattern", "Pattern for filtering image folder names").release();
	}
	AddOperationParameter(*operation, "file_pattern", "Pattern for filtering image file names").release();
	operation.release();

	operation = CreateOperation(*operation_info, IMAGE_FILE_INFO, "Get information on image file");
	AddOperationParameter(*operation, "file", "Image file name", "", true).release();
	operation.release();

	CreateOperation(*operation_info, LOG_LEVEL_INFO, "Get log level information").release();

	CreateOperation(*operation_info, NETWORK_INTERFACES_INFO, "Get the available network interfaces").release();

	CreateOperation(*operation_info, MAPPING_INFO, "Get mapping of extensions to device types").release();

	CreateOperation(*operation_info, RESERVED_IDS_INFO, "Get list of reserved device IDs").release();

	operation = CreateOperation(*operation_info, DEFAULT_FOLDER, "Set default image file folder");
	AddOperationParameter(*operation, "folder", "Default image file folder name", "", true).release();
	operation.release();

	operation = CreateOperation(*operation_info, LOG_LEVEL, "Set log level");
	AddOperationParameter(*operation, "level", "New log level", "", true).release();
	operation.release();

	operation = CreateOperation(*operation_info, RESERVE_IDS, "Reserve device IDs");
	AddOperationParameter(*operation, "ids", "Comma-separated device ID list", "", true).release();
	operation.release();

	operation = CreateOperation(*operation_info, SHUT_DOWN, "Shut down or reboot");
	auto parameter = AddOperationParameter(*operation, "mode", "Shutdown mode", "", true).release();
	parameter->add_permitted_values("piscsi");
	// System shutdown/reboot requires root permissions
	if (!getuid()) {
		parameter->add_permitted_values("system");
		parameter->add_permitted_values("reboot");
	}
	operation.release();

	operation = CreateOperation(*operation_info, CREATE_IMAGE, "Create an image file");
	AddOperationParameter(*operation, "file", "Image file name", "", true).release();
	AddOperationParameter(*operation, "size", "Image file size in bytes", "", true).release();
	parameter = AddOperationParameter(*operation, "read_only",  "Read-only flag", "false").release();
	parameter->add_permitted_values("true");
	parameter->add_permitted_values("false");
	operation.release();

	operation = CreateOperation(*operation_info, DELETE_IMAGE, "Delete image file");
	AddOperationParameter(*operation, "file", "Image file name", "", true).release();
	operation.release();

	operation = CreateOperation(*operation_info, RENAME_IMAGE, "Rename image file");
	AddOperationParameter(*operation, "from", "Source image file name", "", true).release();
	AddOperationParameter(*operation, "to", "Destination image file name", "", true).release();
	operation.release();

	operation = CreateOperation(*operation_info, COPY_IMAGE, "Copy image file");
	AddOperationParameter(*operation, "from", "Source image file name", "", true).release();
	AddOperationParameter(*operation, "to", "Destination image file name", "", true).release();
	parameter = AddOperationParameter(*operation, "read_only", "Read-only flag", "false").release();
	parameter->add_permitted_values("true");
	parameter->add_permitted_values("false");
	operation.release();

	operation = CreateOperation(*operation_info, PROTECT_IMAGE, "Write-protect image file");
	AddOperationParameter(*operation, "file", "Image file name", "", true).release();
	operation.release();

	operation = CreateOperation(*operation_info, UNPROTECT_IMAGE, "Make image file writable");
	AddOperationParameter(*operation, "file", "Image file name", "", true).release();
	operation.release();

	operation = CreateOperation(*operation_info, CHECK_AUTHENTICATION, "Check whether an authentication token is valid");
	AddOperationParameter(*operation, "token", "Authentication token to be checked", "", true).release();
	operation.release();

	CreateOperation(*operation_info, OPERATION_INFO, "Get operation meta data").release();

	result.set_status(true);

	return operation_info;
}

unique_ptr<PbOperationMetaData> PiscsiResponse::CreateOperation(PbOperationInfo& operation_info, const PbOperation& operation,
		const string& description) const
{
	auto meta_data = make_unique<PbOperationMetaData>();
	meta_data->set_server_side_name(PbOperation_Name(operation));
	meta_data->set_description(description);
	int ordinal = PbOperation_descriptor()->FindValueByName(PbOperation_Name(operation))->index();
	(*operation_info.mutable_operations())[ordinal] = *meta_data.release();
	return unique_ptr<PbOperationMetaData>(&(*operation_info.mutable_operations())[ordinal]);
}

unique_ptr<PbOperationParameter> PiscsiResponse::AddOperationParameter(PbOperationMetaData& meta_data,
		const string& name, const string& description, const string& default_value, bool is_mandatory) const
{
	auto parameter = unique_ptr<PbOperationParameter>(meta_data.add_parameters());
	parameter->set_name(name);
	parameter->set_description(description);
	parameter->set_default_value(default_value);
	parameter->set_is_mandatory(is_mandatory);

	return parameter;
}

set<id_set> PiscsiResponse::MatchDevices(const unordered_set<shared_ptr<PrimaryDevice>>& devices, PbResult& result,
		const PbCommand& command) const
{
	set<id_set> id_sets;

	for (const auto& device : command.devices()) {
		bool has_device = false;
		for (const auto& d : devices) {
			if (d->GetId() == device.id() && d->GetLun() == device.unit()) {
				id_sets.insert({ device.id(), device.unit() });
				has_device = true;
				break;
			}
		}

		if (!has_device) {
			id_sets.clear();

			result.set_status(false);
			result.set_msg("No device for ID " + to_string(device.id()) + ", unit " + to_string(device.unit()));

			break;
		}
	}

	return id_sets;
}

bool PiscsiResponse::ValidateImageFile(const path& path)
{
	if (path.filename().string().starts_with(".")) {
		return false;
	}

	filesystem::path p(path);

	// Follow symlink
	if (is_symlink(p)) {
		p = read_symlink(p);
		if (!exists(p)) {
			spdlog::warn("Image file symlink '" + path.string() + "' is broken");
			return false;
		}
	}

	if (is_directory(p) || (is_other(p) && !is_block_file(p))) {
		return false;
	}

	if (!is_block_file(p) && file_size(p) < 256) {
		spdlog::warn("Image file '" + p.string() + "' is invalid");
		return false;
	}

	return true;
}

bool PiscsiResponse::FilterMatches(const string& input, string_view pattern_lower)
{
	if (!pattern_lower.empty()) {
		string name_lower = input;
		ranges::transform(name_lower, name_lower.begin(), ::tolower);

		if (name_lower.find(pattern_lower) == string::npos) {
			return false;
		}
	}

	return true;
}
