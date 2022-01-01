//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "devices/file_support.h"
#include "devices/disk.h"
#include "devices/device_factory.h"
#include "devices/device.h"
#include "protobuf_util.h"
#include "rascsi_version.h"
#include "rascsi_interface.pb.h"
#include "rascsi_image.h"
#include "rascsi_response.h"

using namespace rascsi_interface;
using namespace protobuf_util;

RascsiResponse::RascsiResponse(DeviceFactory *device_factory, const RascsiImage *rascsi_image)
{
	this->device_factory = device_factory;
	this->rascsi_image = rascsi_image;
}

PbDeviceProperties *RascsiResponse::GetDeviceProperties(const Device *device)
{
	PbDeviceProperties *properties = new PbDeviceProperties();

	properties->set_luns(device->GetSupportedLuns());
	properties->set_read_only(device->IsReadOnly());
	properties->set_protectable(device->IsProtectable());
	properties->set_stoppable(device->IsStoppable());
	properties->set_removable(device->IsRemovable());
	properties->set_lockable(device->IsLockable());
	properties->set_supports_file(dynamic_cast<const FileSupport *>(device));
	properties->set_supports_params(device->SupportsParams());

	PbDeviceType t = UNDEFINED;
	PbDeviceType_Parse(device->GetType(), &t);

	if (device->SupportsParams()) {
		for (const auto& param : device_factory->GetDefaultParams(t)) {
			auto& map = *properties->mutable_default_params();
			map[param.first] = param.second;
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
	Device *device = device_factory->CreateDevice(type, "");
	type_properties->set_allocated_properties(GetDeviceProperties(device));
	delete device;
}

void RascsiResponse::GetAllDeviceTypeProperties(PbDeviceTypesInfo& device_types_info)
{
	GetDeviceTypeProperties(device_types_info, SAHD);
	GetDeviceTypeProperties(device_types_info, SCHD);
	GetDeviceTypeProperties(device_types_info, SCRM);
	GetDeviceTypeProperties(device_types_info, SCMO);
	GetDeviceTypeProperties(device_types_info, SCCD);
	GetDeviceTypeProperties(device_types_info, SCBR);
	GetDeviceTypeProperties(device_types_info, SCDP);
}

void RascsiResponse::GetDevice(const Device *device, PbDevice *pb_device)
{
	pb_device->set_id(device->GetId());
	pb_device->set_unit(device->GetLun());
	pb_device->set_vendor(device->GetVendor());
	pb_device->set_product(device->GetProduct());
	pb_device->set_revision(device->GetRevision());

	PbDeviceType type = UNDEFINED;
	PbDeviceType_Parse(device->GetType(), &type);
	pb_device->set_type(type);

    pb_device->set_allocated_properties(GetDeviceProperties(device));

    PbDeviceStatus *status = new PbDeviceStatus();
	pb_device->set_allocated_status(status);
	status->set_protected_(device->IsProtected());
	status->set_stopped(device->IsStopped());
	status->set_removed(device->IsRemoved());
	status->set_locked(device->IsLocked());

	if (device->SupportsParams()) {
		for (const auto& param : device->GetParams()) {
			AddParam(*pb_device, param.first, param.second);
		}
	}

	const Disk *disk = dynamic_cast<const Disk*>(device);
    if (disk) {
    	pb_device->set_block_size(device->IsRemoved()? 0 : disk->GetSectorSizeInBytes());
    	pb_device->set_block_count(device->IsRemoved() ? 0: disk->GetBlockCount());
    }

    const FileSupport *file_support = dynamic_cast<const FileSupport *>(device);
	if (file_support) {
		Filepath filepath;
		file_support->GetPath(filepath);
		PbImageFile *image_file = new PbImageFile();
		GetImageFile(image_file, device->IsRemovable() && !device->IsReady() ? "" : filepath.GetPath());
		pb_device->set_allocated_file(image_file);
	}
}

bool RascsiResponse::GetImageFile(PbImageFile *image_file, const string& filename)
{
	if (!filename.empty()) {
		image_file->set_name(filename);
		image_file->set_type(device_factory->GetTypeForFile(filename));

		string f = filename[0] == '/' ? filename : rascsi_image->GetDefaultImageFolder() + "/" + filename;

		image_file->set_read_only(access(f.c_str(), W_OK));

		struct stat st;
		if (!stat(f.c_str(), &st) && !S_ISDIR(st.st_mode)) {
			image_file->set_size(st.st_size);
			return true;
		}
	}

	return false;
}

void RascsiResponse::GetAvailableImages(PbImageFilesInfo& image_files_info, const string& default_image_folder,
		const string& folder, const string& folder_pattern, const string& file_pattern, int scan_depth) {
	string folder_pattern_lower = folder_pattern;
	transform(folder_pattern_lower.begin(), folder_pattern_lower.end(), folder_pattern_lower.begin(), ::tolower);

	string file_pattern_lower = file_pattern;
	transform(file_pattern_lower.begin(), file_pattern_lower.end(), file_pattern_lower.begin(), ::tolower);

	if (scan_depth-- >= 0) {
		DIR *d = opendir(folder.c_str());
		if (d) {
			struct dirent *dir;
			while ((dir = readdir(d))) {
				bool is_supported_type = dir->d_type == DT_REG || dir->d_type == DT_DIR || dir->d_type == DT_LNK || dir->d_type == DT_BLK;
				if (is_supported_type && dir->d_name[0] != '.') {
					string name_lower = dir->d_name;
					if (!file_pattern.empty()) {
						transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
					}

					string filename = folder + "/" + dir->d_name;

					struct stat st;
					if (dir->d_type == DT_REG && !stat(filename.c_str(), &st)) {
						if (!st.st_size) {
							LOGWARN("File '%s' in image folder '%s' has a size of 0 bytes", dir->d_name, folder.c_str());
							continue;
						}
					} else if (dir->d_type == DT_LNK && stat(filename.c_str(), &st)) {
						LOGWARN("Symlink '%s' in image folder '%s' is broken", dir->d_name, folder.c_str());
						continue;
					} else if (dir->d_type == DT_DIR) {
						if (folder_pattern_lower.empty() || name_lower.find(folder_pattern_lower) != string::npos) {
							GetAvailableImages(image_files_info, default_image_folder, filename, folder_pattern,
									file_pattern, scan_depth);
						}
						continue;
					}

					if (file_pattern_lower.empty() || name_lower.find(file_pattern_lower) != string::npos) {
						PbImageFile *image_file = new PbImageFile();
						if (GetImageFile(image_file, filename)) {
							GetImageFile(image_files_info.add_image_files(), filename.substr(default_image_folder.length() + 1));
						}
						delete image_file;
					}
				}
			}

			closedir(d);
		}
	}
}

PbImageFilesInfo *RascsiResponse::GetAvailableImages(PbResult& result, const string& folder_pattern,
		const string& file_pattern, int scan_depth)
{
	PbImageFilesInfo *image_files_info = new PbImageFilesInfo();

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

	result.set_status(true);
}

PbReservedIdsInfo *RascsiResponse::GetReservedIds(PbResult& result, const set<int>& ids)
{
	PbReservedIdsInfo *reserved_ids_info = new PbReservedIdsInfo();
	for (int id : ids) {
		reserved_ids_info->add_ids(id);
	}

	result.set_status(true);

	return reserved_ids_info;
}

void RascsiResponse::GetDevices(PbServerInfo& server_info, const vector<Device *>& devices)
{
	for (const Device *device : devices) {
		// Skip if unit does not exist or is not assigned
		if (device) {
			PbDevice *pb_device = server_info.mutable_devices_info()->add_devices();
			GetDevice(device, pb_device);
		}
	}
}

void RascsiResponse::GetDevicesInfo(PbResult& result, const PbCommand& command, const vector<Device *>& devices,
		int unit_count)
{
	set<id_set> id_sets;
	if (!command.devices_size()) {
		for (const Device *device : devices) {
			if (device) {
				id_sets.insert(make_pair(device->GetId(), device->GetLun()));
			}
		}
	}
	else {
		for (const auto& device : command.devices()) {
			if (devices[device.id() * unit_count + device.unit()]) {
				id_sets.insert(make_pair(device.id(), device.unit()));
			}
			else {
				result.set_status(false);
				result.set_msg("No device for ID " + to_string(device.id()) + ", unit " + to_string(device.unit()));
				return;
			}
		}
	}

	PbDevicesInfo *devices_info = new PbDevicesInfo();
	result.set_allocated_devices_info(devices_info);

	for (const auto& id_set : id_sets) {
		const Device *device = devices[id_set.first * unit_count + id_set.second];
		GetDevice(device, devices_info->add_devices());
	}

	result.set_status(true);
}

PbDeviceTypesInfo *RascsiResponse::GetDeviceTypesInfo(PbResult& result, const PbCommand& command)
{
	PbDeviceTypesInfo *device_types_info = new PbDeviceTypesInfo();

	GetAllDeviceTypeProperties(*device_types_info);

	result.set_status(true);

	return device_types_info;
}

PbServerInfo *RascsiResponse::GetServerInfo(PbResult& result, const vector<Device *>& devices, const set<int>& reserved_ids,
		const string& current_log_level, const string& folder_pattern, const string& file_pattern, int scan_depth)
{
	PbServerInfo *server_info = new PbServerInfo();

	server_info->set_allocated_version_info(GetVersionInfo(result));
	server_info->set_allocated_log_level_info(GetLogLevelInfo(result, current_log_level));
	GetAllDeviceTypeProperties(*server_info->mutable_device_types_info());
	GetAvailableImages(result, *server_info, folder_pattern, file_pattern, scan_depth);
	server_info->set_allocated_network_interfaces_info(GetNetworkInterfacesInfo(result));
	server_info->set_allocated_mapping_info(GetMappingInfo(result));
	GetDevices(*server_info, devices);
	server_info->set_allocated_reserved_ids_info(GetReservedIds(result, reserved_ids));
	server_info->set_allocated_operation_info(GetOperationInfo(result, scan_depth));

	result.set_status(true);

	return server_info;
}

PbVersionInfo *RascsiResponse::GetVersionInfo(PbResult& result)
{
	PbVersionInfo *version_info = new PbVersionInfo();

	version_info->set_major_version(rascsi_major_version);
	version_info->set_minor_version(rascsi_minor_version);
	version_info->set_patch_version(rascsi_patch_version);

	result.set_status(true);

	return version_info;
}

PbLogLevelInfo *RascsiResponse::GetLogLevelInfo(PbResult& result, const string& current_log_level)
{
	PbLogLevelInfo *log_level_info = new PbLogLevelInfo();

	for (const auto& log_level : log_levels) {
		log_level_info->add_log_levels(log_level);
	}

	log_level_info->set_current_log_level(current_log_level);

	result.set_status(true);

	return log_level_info;
}

PbNetworkInterfacesInfo *RascsiResponse::GetNetworkInterfacesInfo(PbResult& result)
{
	PbNetworkInterfacesInfo *network_interfaces_info = new PbNetworkInterfacesInfo();

	for (const auto& network_interface : device_factory->GetNetworkInterfaces()) {
		network_interfaces_info->add_name(network_interface);
	}

	result.set_status(true);

	return network_interfaces_info;
}

PbMappingInfo *RascsiResponse::GetMappingInfo(PbResult& result)
{
	PbMappingInfo *mapping_info = new PbMappingInfo();

	for (const auto& mapping : device_factory->GetExtensionMapping()) {
		(*mapping_info->mutable_mapping())[mapping.first] = mapping.second;
	}

	result.set_status(true);

	return mapping_info;
}

PbOperationInfo *RascsiResponse::GetOperationInfo(PbResult& result, int depth)
{
	PbOperationInfo *operation_info = new PbOperationInfo();

	PbOperationMetaData *meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "name", "Image file name in case of a mass storage device");
	AddOperationParameter(meta_data, "interfaces", "Comma-separated prioritized network interface list");
	CreateOperation(operation_info, meta_data, ATTACH, "Attach device, device-specific parameters are required");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, DETACH, "Detach device, device-specific parameters are required");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, DETACH_ALL, "Detach all devices");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, START, "Start device, device-specific parameters are required");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, STOP, "Stop device, device-specific parameters are required");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "file", "Image file name", "", true);
	CreateOperation(operation_info, meta_data, INSERT, "Insert medium, device-specific parameters are required");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, EJECT, "Eject medium, device-specific parameters are required");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, PROTECT, "Protect medium, device-specific parameters are required");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, UNPROTECT, "Unprotect medium, device-specific parameters are required");

	meta_data = new PbOperationMetaData();
	if (depth) {
		AddOperationParameter(meta_data, "folder_pattern", "Pattern for filtering image folder names");
	}
	AddOperationParameter(meta_data, "file_pattern", "Pattern for filtering image file names");
	CreateOperation(operation_info, meta_data, SERVER_INFO, "Get rascsi server information");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, VERSION_INFO, "Get rascsi server version");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, DEVICES_INFO, "Get information on attached devices");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, DEVICE_TYPES_INFO, "Get device properties by device type");

	meta_data = new PbOperationMetaData();
	if (depth) {
		AddOperationParameter(meta_data, "folder_pattern", "Pattern for filtering image folder names");
	}
	AddOperationParameter(meta_data, "file_pattern", "Pattern for filtering image file names");
	CreateOperation(operation_info, meta_data, DEFAULT_IMAGE_FILES_INFO, "Get information on available image files");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "file", "Image file name", "", true);
	CreateOperation(operation_info, meta_data, IMAGE_FILE_INFO, "Get information on image file");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, LOG_LEVEL_INFO, "Get log level information");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, NETWORK_INTERFACES_INFO, "Get the available network interfaces");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, MAPPING_INFO, "Get mapping of extensions to device types");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, RESERVED_IDS_INFO, "Get list of reserved device IDs");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "folder", "Default image file folder name", "", true);
	CreateOperation(operation_info, meta_data, DEFAULT_FOLDER, "Set default image file folder");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "level", "New log level", "", true);
	CreateOperation(operation_info, meta_data, LOG_LEVEL, "Set log level");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "ids", "Comma-separated device ID list", "", true);
	CreateOperation(operation_info, meta_data, RESERVE_IDS, "Reserve device IDs");

	meta_data = new PbOperationMetaData();
	PbOperationParameter *parameter = AddOperationParameter(meta_data, "mode", "Shutdown mode", "", true);
	parameter->add_permitted_values("rascsi");
	// System shutdown/reboot requires root permissions
	if (!getuid()) {
		parameter->add_permitted_values("system");
		parameter->add_permitted_values("reboot");
	}
	CreateOperation(operation_info, meta_data, SHUT_DOWN, "Shut down or reboot");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "file", "Image file name", "", true);
	AddOperationParameter(meta_data, "size", "Image file size in bytes", "", true);
	parameter = AddOperationParameter(meta_data, "read_only",  "Read-only flag", "false");
	parameter->add_permitted_values("true");
	parameter->add_permitted_values("false");
	CreateOperation(operation_info, meta_data, CREATE_IMAGE, "Create an image file");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "file", "Image file name", "", true);
	CreateOperation(operation_info, meta_data, DELETE_IMAGE, "Delete image file");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "from", "Source image file name", "", true);
	AddOperationParameter(meta_data, "to", "Destination image file name", "", true);
	CreateOperation(operation_info, meta_data, RENAME_IMAGE, "Rename image file");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "from", "Source image file name", "", true);
	AddOperationParameter(meta_data, "to", "Destination image file name", "", true);
	parameter = AddOperationParameter(meta_data, "read_only", "Read-only flag", "false");
	parameter->add_permitted_values("true");
	parameter->add_permitted_values("false");
	CreateOperation(operation_info, meta_data, COPY_IMAGE, "Copy image file");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "file", "Image file name", "", true);
	CreateOperation(operation_info, meta_data, PROTECT_IMAGE, "Write-protect image file");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "file", "Image file name", "", true);
	CreateOperation(operation_info, meta_data, UNPROTECT_IMAGE, "Make image file writable");

	meta_data = new PbOperationMetaData();
	AddOperationParameter(meta_data, "token", "Authentication token to be checked", "", true);
	CreateOperation(operation_info, meta_data, CHECK_AUTHENTICATION, "Check whether an authentication token is valid");

	meta_data = new PbOperationMetaData();
	CreateOperation(operation_info, meta_data, OPERATION_INFO, "Get operation meta data");

	// Ensure that the complete set of operations is covered
	assert(operation_info->operations_size() == PbOperation_ARRAYSIZE - 1);

	result.set_status(true);

	return operation_info;
}

void RascsiResponse::CreateOperation(PbOperationInfo *operation_info, PbOperationMetaData *meta_data,
		const PbOperation& operation, const string& description)
{
	meta_data->set_server_side_name(PbOperation_Name(operation));
	meta_data->set_description(description);
	int ordinal = PbOperation_descriptor()->FindValueByName(PbOperation_Name(operation))->index();
	(*operation_info->mutable_operations())[ordinal] = *meta_data;
}

PbOperationParameter *RascsiResponse::AddOperationParameter(PbOperationMetaData *meta_data, const string& name,
		const string& description, const string& default_value, bool is_mandatory)
{
	PbOperationParameter *parameter = meta_data->add_parameters();
	parameter->set_name(name);
	parameter->set_description(description);
	parameter->set_default_value(default_value);
	parameter->set_is_mandatory(is_mandatory);

	return parameter;
}
