//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "devices/file_support.h"
#include "devices/disk.h"
#include "devices/device_factory.h"
#include "command_util.h"
#include "rascsi_version.h"
#include "rascsi_interface.pb.h"
#include "rascsi_image.h"
#include "rascsi_response.h"

using namespace rascsi_interface;
using namespace command_util;

PbDeviceProperties *RascsiResponse::GetDeviceProperties(const Device& device)
{
	auto properties = make_unique<PbDeviceProperties>().release();

	properties->set_luns(ScsiController::LUN_MAX);
	properties->set_read_only(device.IsReadOnly());
	properties->set_protectable(device.IsProtectable());
	properties->set_stoppable(device.IsStoppable());
	properties->set_removable(device.IsRemovable());
	properties->set_lockable(device.IsLockable());
	properties->set_supports_file(dynamic_cast<const FileSupport *>(&device) != nullptr);
	properties->set_supports_params(device.SupportsParams());

	PbDeviceType t = UNDEFINED;
	PbDeviceType_Parse(device.GetType(), &t);

	if (device.SupportsParams()) {
		for (const auto& [key, value] : device_factory->GetDefaultParams(t)) {
			auto& map = *properties->mutable_default_params();
			map[key] = value;
		}
	}

	for (const auto& block_size : device_factory->GetSectorSizes(t)) {
		properties->add_block_sizes(block_size);
	}

	return properties;
}

void RascsiResponse::GetDeviceTypeProperties(PbDeviceTypesInfo& device_types_info, PbDeviceType type)
{
	PbDeviceTypeProperties *type_properties = device_types_info.add_properties();
	type_properties->set_type(type);
	const PrimaryDevice *device = device_factory->CreateDevice(type, "", -1);
	type_properties->set_allocated_properties(GetDeviceProperties(*device));
	device_factory->DeleteDevice(*device); //NOSONAR The allocated memory is managed by protobuf
}

void RascsiResponse::GetAllDeviceTypeProperties(PbDeviceTypesInfo& device_types_info)
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

void RascsiResponse::GetDevice(const Device& device, PbDevice *pb_device)
{
	pb_device->set_id(device.GetId());
	pb_device->set_unit(device.GetLun());
	pb_device->set_vendor(device.GetVendor());
	pb_device->set_product(device.GetProduct());
	pb_device->set_revision(device.GetRevision());

	PbDeviceType type = UNDEFINED;
	PbDeviceType_Parse(device.GetType(), &type);
	pb_device->set_type(type);

    pb_device->set_allocated_properties(GetDeviceProperties(device));

    auto status = make_unique<PbDeviceStatus>().release();
	pb_device->set_allocated_status(status);
	status->set_protected_(device.IsProtected());
	status->set_stopped(device.IsStopped());
	status->set_removed(device.IsRemoved());
	status->set_locked(device.IsLocked());

	if (device.SupportsParams()) { //NOSONAR The allocated memory is managed by protobuf
		for (const auto& [key, value] : device.GetParams()) {
			AddParam(*pb_device, key, value);
		}
	}

    if (const auto disk = dynamic_cast<const Disk*>(&device); disk) {
    	pb_device->set_block_size(device.IsRemoved()? 0 : disk->GetSectorSizeInBytes());
    	pb_device->set_block_count(device.IsRemoved() ? 0: disk->GetBlockCount());
    }

    const auto file_support = dynamic_cast<const FileSupport *>(&device);
	if (file_support) {
		Filepath filepath;
		file_support->GetPath(filepath);
		auto image_file = make_unique<PbImageFile>().release();
		GetImageFile(image_file, device.IsRemovable() && !device.IsReady() ? "" : filepath.GetPath());
		pb_device->set_allocated_file(image_file);
	}
} //NOSONAR The allocated memory is managed by protobuf

bool RascsiResponse::GetImageFile(PbImageFile *image_file, const string& filename) const
{
	if (!filename.empty()) {
		image_file->set_name(filename);
		image_file->set_type(device_factory->GetTypeForFile(filename));

		string f = filename[0] == '/' ? filename : rascsi_image->GetDefaultImageFolder() + "/" + filename;

		image_file->set_read_only(access(f.c_str(), W_OK));

		if (struct stat st; !stat(f.c_str(), &st) && !S_ISDIR(st.st_mode)) {
			image_file->set_size(st.st_size);
			return true;
		}
	}

	return false;
}

void RascsiResponse::GetAvailableImages(PbImageFilesInfo& image_files_info, string_view default_image_folder,
		const string& folder, const string& folder_pattern, const string& file_pattern, int scan_depth) {
	if (scan_depth-- < 0) {
		return;
	}

	string folder_pattern_lower = folder_pattern;
	transform(folder_pattern_lower.begin(), folder_pattern_lower.end(), folder_pattern_lower.begin(), ::tolower);

	string file_pattern_lower = file_pattern;
	transform(file_pattern_lower.begin(), file_pattern_lower.end(), file_pattern_lower.begin(), ::tolower);

	DIR *d = opendir(folder.c_str());
	if (d == nullptr) {
		return;
	}

	const dirent *dir;
	while ((dir = readdir(d))) {
		string filename = GetNextImageFile(dir, folder);
		if (filename.empty()) {
			continue;
		}

		string name_lower = dir->d_name;
		if (!file_pattern.empty()) {
			transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
		}

		if (dir->d_type == DT_DIR) {
			if (folder_pattern_lower.empty() || name_lower.find(folder_pattern_lower) != string::npos) {
				GetAvailableImages(image_files_info, default_image_folder, filename, folder_pattern,
						file_pattern, scan_depth);
			}

			continue;
		}

		if (file_pattern_lower.empty() || name_lower.find(file_pattern_lower) != string::npos) {
			if (auto image_file = make_unique<PbImageFile>(); GetImageFile(image_file.get(), filename)) {
				GetImageFile(image_files_info.add_image_files(), filename.substr(default_image_folder.length() + 1));
			}
		}
	}

	closedir(d);
}

PbImageFilesInfo *RascsiResponse::GetAvailableImages(PbResult& result, const string& folder_pattern,
		const string& file_pattern, int scan_depth)
{
	auto image_files_info = make_unique<PbImageFilesInfo>().release();

	string default_image_folder = rascsi_image->GetDefaultImageFolder();
	image_files_info->set_default_image_folder(default_image_folder);
	image_files_info->set_depth(scan_depth);

	GetAvailableImages(*image_files_info, default_image_folder, default_image_folder, folder_pattern,
			file_pattern, scan_depth);

	result.set_status(true);

	return image_files_info;
}

void RascsiResponse::GetAvailableImages(PbResult& result, PbServerInfo& server_info, const string& folder_pattern,
		const string& file_pattern, int scan_depth)
{
	PbImageFilesInfo *image_files_info = GetAvailableImages(result, folder_pattern, file_pattern, scan_depth);
	image_files_info->set_default_image_folder(rascsi_image->GetDefaultImageFolder());
	server_info.set_allocated_image_files_info(image_files_info);

	result.set_status(true); //NOSONAR The allocated memory is managed by protobuf
}

PbReservedIdsInfo *RascsiResponse::GetReservedIds(PbResult& result, const unordered_set<int>& ids)
{
	auto reserved_ids_info = make_unique<PbReservedIdsInfo>().release();
	for (int id : ids) {
		reserved_ids_info->add_ids(id);
	}

	result.set_status(true);

	return reserved_ids_info;
}

void RascsiResponse::GetDevices(PbServerInfo& server_info)
{
	for (const Device *device : device_factory->GetAllDevices()) {
		PbDevice *pb_device = server_info.mutable_devices_info()->add_devices();
		GetDevice(*device, pb_device);
	}
}

void RascsiResponse::GetDevicesInfo(PbResult& result, const PbCommand& command)
{
	set<id_set> id_sets;
	if (!command.devices_size()) {
		for (const Device *device : device_factory->GetAllDevices()) {
			id_sets.insert(make_pair(device->GetId(), device->GetLun()));
		}
	}
	else {
		for (const auto& device : command.devices()) {
			if (device_factory->GetDeviceByIdAndLun(device.id(), device.unit())) {
				id_sets.insert(make_pair(device.id(), device.unit()));
			}
			else {
				result.set_status(false);
				result.set_msg("No device for ID " + to_string(device.id()) + ", unit " + to_string(device.unit()));
				return;
			}
		}
	}

	auto devices_info = make_unique<PbDevicesInfo>().release();
	result.set_allocated_devices_info(devices_info);

	for (const auto& [id, lun] : id_sets) {
		GetDevice(*device_factory->GetDeviceByIdAndLun(id, lun), devices_info->add_devices());
	}

	result.set_status(true);
}

PbDeviceTypesInfo *RascsiResponse::GetDeviceTypesInfo(PbResult& result)
{
	auto device_types_info = make_unique<PbDeviceTypesInfo>().release();

	GetAllDeviceTypeProperties(*device_types_info);

	result.set_status(true);

	return device_types_info;
}

PbServerInfo *RascsiResponse::GetServerInfo(PbResult& result, const unordered_set<int>& reserved_ids,
		const string& current_log_level, const string& folder_pattern, const string& file_pattern, int scan_depth)
{
	auto server_info = make_unique<PbServerInfo>().release();

	server_info->set_allocated_version_info(GetVersionInfo(result));
	server_info->set_allocated_log_level_info(GetLogLevelInfo(result, current_log_level)); //NOSONAR The allocated memory is managed by protobuf
	GetAllDeviceTypeProperties(*server_info->mutable_device_types_info()); //NOSONAR The allocated memory is managed by protobuf
	GetAvailableImages(result, *server_info, folder_pattern, file_pattern, scan_depth);
	server_info->set_allocated_network_interfaces_info(GetNetworkInterfacesInfo(result));
	server_info->set_allocated_mapping_info(GetMappingInfo(result)); //NOSONAR The allocated memory is managed by protobuf
	GetDevices(*server_info); //NOSONAR The allocated memory is managed by protobuf
	server_info->set_allocated_reserved_ids_info(GetReservedIds(result, reserved_ids));
	server_info->set_allocated_operation_info(GetOperationInfo(result, scan_depth)); //NOSONAR The allocated memory is managed by protobuf

	result.set_status(true);

	return server_info;
}

PbVersionInfo *RascsiResponse::GetVersionInfo(PbResult& result)
{
	auto version_info = make_unique<PbVersionInfo>().release();

	version_info->set_major_version(rascsi_major_version);
	version_info->set_minor_version(rascsi_minor_version);
	version_info->set_patch_version(rascsi_patch_version);

	result.set_status(true);

	return version_info;
}

PbLogLevelInfo *RascsiResponse::GetLogLevelInfo(PbResult& result, const string& current_log_level)
{
	auto log_level_info = make_unique<PbLogLevelInfo>().release();

	for (const auto& log_level : log_levels) {
		log_level_info->add_log_levels(log_level);
	}

	log_level_info->set_current_log_level(current_log_level);

	result.set_status(true);

	return log_level_info;
}

PbNetworkInterfacesInfo *RascsiResponse::GetNetworkInterfacesInfo(PbResult& result)
{
	auto network_interfaces_info = make_unique<PbNetworkInterfacesInfo>().release();

	for (const auto& network_interface : device_factory->GetNetworkInterfaces()) {
		network_interfaces_info->add_name(network_interface);
	}

	result.set_status(true);

	return network_interfaces_info;
}

PbMappingInfo *RascsiResponse::GetMappingInfo(PbResult& result)
{
	auto mapping_info = make_unique<PbMappingInfo>().release();

	for (const auto& [name, type] : device_factory->GetExtensionMapping()) {
		(*mapping_info->mutable_mapping())[name] = type;
	}

	result.set_status(true);

	return mapping_info;
}

PbOperationInfo *RascsiResponse::GetOperationInfo(PbResult& result, int depth)
{
	auto operation_info = make_unique<PbOperationInfo>();

	auto operation = CreateOperation(*operation_info, ATTACH, "Attach device, device-specific parameters are required");
	AddOperationParameter(*operation, "name", "Image file name in case of a mass storage device");
	AddOperationParameter(*operation, "interface", "Comma-separated prioritized network interface list");
	AddOperationParameter(*operation, "inet", "IP address and netmask of the network bridge");
	AddOperationParameter(*operation, "cmd", "Print command for the printer device");
	AddOperationParameter(*operation, "timeout", "Reservation timeout for the printer device in seconds");

	CreateOperation(*operation_info, DETACH, "Detach device, device-specific parameters are required");

	CreateOperation(*operation_info, DETACH_ALL, "Detach all devices");

	CreateOperation(*operation_info, START, "Start device, device-specific parameters are required");

	CreateOperation(*operation_info, STOP, "Stop device, device-specific parameters are required");

	operation = CreateOperation(*operation_info, INSERT, "Insert medium, device-specific parameters are required");
	AddOperationParameter(*operation, "file", "Image file name", "", true);

	CreateOperation(*operation_info, EJECT, "Eject medium, device-specific parameters are required");

	CreateOperation(*operation_info, PROTECT, "Protect medium, device-specific parameters are required");

	CreateOperation(*operation_info, UNPROTECT, "Unprotect medium, device-specific parameters are required");

	operation = CreateOperation(*operation_info, SERVER_INFO, "Get rascsi server information");
	if (depth) {
		AddOperationParameter(*operation, "folder_pattern", "Pattern for filtering image folder names");
	}
	AddOperationParameter(*operation, "file_pattern", "Pattern for filtering image file names");

	CreateOperation(*operation_info, VERSION_INFO, "Get rascsi server version");

	CreateOperation(*operation_info, DEVICES_INFO, "Get information on attached devices");

	CreateOperation(*operation_info, DEVICE_TYPES_INFO, "Get device properties by device type");

	operation = CreateOperation(*operation_info, DEFAULT_IMAGE_FILES_INFO, "Get information on available image files");
	if (depth) {
		AddOperationParameter(*operation, "folder_pattern", "Pattern for filtering image folder names");
	}
	AddOperationParameter(*operation, "file_pattern", "Pattern for filtering image file names");

	operation = CreateOperation(*operation_info, IMAGE_FILE_INFO, "Get information on image file");
	AddOperationParameter(*operation, "file", "Image file name", "", true);

	CreateOperation(*operation_info, LOG_LEVEL_INFO, "Get log level information");

	CreateOperation(*operation_info, NETWORK_INTERFACES_INFO, "Get the available network interfaces");

	CreateOperation(*operation_info, MAPPING_INFO, "Get mapping of extensions to device types");

	CreateOperation(*operation_info, RESERVED_IDS_INFO, "Get list of reserved device IDs");

	operation = CreateOperation(*operation_info, DEFAULT_FOLDER, "Set default image file folder");
	AddOperationParameter(*operation, "folder", "Default image file folder name", "", true);

	operation = CreateOperation(*operation_info, LOG_LEVEL, "Set log level");
	AddOperationParameter(*operation, "level", "New log level", "", true);

	operation = CreateOperation(*operation_info, RESERVE_IDS, "Reserve device IDs");
	AddOperationParameter(*operation, "ids", "Comma-separated device ID list", "", true);

	operation = CreateOperation(*operation_info, SHUT_DOWN, "Shut down or reboot");
	PbOperationParameter *parameter = AddOperationParameter(*operation, "mode", "Shutdown mode", "", true);
	parameter->add_permitted_values("rascsi");
	// System shutdown/reboot requires root permissions
	if (!getuid()) {
		parameter->add_permitted_values("system");
		parameter->add_permitted_values("reboot");
	}

	operation = CreateOperation(*operation_info, CREATE_IMAGE, "Create an image file");
	AddOperationParameter(*operation, "file", "Image file name", "", true);
	AddOperationParameter(*operation, "size", "Image file size in bytes", "", true);
	parameter = AddOperationParameter(*operation, "read_only",  "Read-only flag", "false");
	parameter->add_permitted_values("true");
	parameter->add_permitted_values("false");

	operation = CreateOperation(*operation_info, DELETE_IMAGE, "Delete image file");
	AddOperationParameter(*operation, "file", "Image file name", "", true);

	operation = CreateOperation(*operation_info, RENAME_IMAGE, "Rename image file");
	AddOperationParameter(*operation, "from", "Source image file name", "", true);
	AddOperationParameter(*operation, "to", "Destination image file name", "", true);

	operation = CreateOperation(*operation_info, COPY_IMAGE, "Copy image file");
	AddOperationParameter(*operation, "from", "Source image file name", "", true);
	AddOperationParameter(*operation, "to", "Destination image file name", "", true);
	parameter = AddOperationParameter(*operation, "read_only", "Read-only flag", "false");
	parameter->add_permitted_values("true");
	parameter->add_permitted_values("false");

	operation = CreateOperation(*operation_info, PROTECT_IMAGE, "Write-protect image file");
	AddOperationParameter(*operation, "file", "Image file name", "", true);

	operation = CreateOperation(*operation_info, UNPROTECT_IMAGE, "Make image file writable");
	AddOperationParameter(*operation, "file", "Image file name", "", true);

	operation = CreateOperation(*operation_info, CHECK_AUTHENTICATION, "Check whether an authentication token is valid");
	AddOperationParameter(*operation, "token", "Authentication token to be checked", "", true);

	CreateOperation(*operation_info, OPERATION_INFO, "Get operation meta data");

	result.set_status(true);

	return operation_info.release();
}

PbOperationMetaData *RascsiResponse::CreateOperation(PbOperationInfo& operation_info, const PbOperation& operation,
		const string& description) const
{
	auto meta_data = make_shared<PbOperationMetaData>();
	meta_data->set_server_side_name(PbOperation_Name(operation));
	meta_data->set_description(description);
	int ordinal = PbOperation_descriptor()->FindValueByName(PbOperation_Name(operation))->index();
	(*operation_info.mutable_operations())[ordinal] = *meta_data.get();
	return &(*operation_info.mutable_operations())[ordinal];
}

PbOperationParameter *RascsiResponse::AddOperationParameter(PbOperationMetaData& meta_data, const string& name,
		const string& description, const string& default_value, bool is_mandatory)
{
	auto parameter = unique_ptr<PbOperationParameter>(meta_data.add_parameters());
	parameter->set_name(name);
	parameter->set_description(description);
	parameter->set_default_value(default_value);
	parameter->set_is_mandatory(is_mandatory);

	return parameter.release();
}

string RascsiResponse::GetNextImageFile(const dirent *dir, const string& folder)
{
	// Ignore unknown folder types and folder names starting with '.'
	if ((dir->d_type != DT_REG && dir->d_type != DT_DIR && dir->d_type != DT_LNK && dir->d_type != DT_BLK)
			|| dir->d_name[0] == '.') {
		return "";
	}

	string filename = folder + "/" + dir->d_name;

	struct stat st;

	bool file_exists = !stat(filename.c_str(), &st);

	if (dir->d_type == DT_REG && file_exists && !st.st_size) {
		LOGWARN("File '%s' in image folder '%s' has a size of 0 bytes", dir->d_name, folder.c_str())
		return "";
	}

	if (dir->d_type == DT_LNK && !file_exists) {
		LOGWARN("Symlink '%s' in image folder '%s' is broken", dir->d_name, folder.c_str())
		return "";
	}

	return filename;
}
