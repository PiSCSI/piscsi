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
#include <sstream>

using namespace rascsi_interface;
using namespace protobuf_util;

RascsiResponse::RascsiResponse(DeviceFactory *device_factory, const RascsiImage *rascsi_image)
{
	this->device_factory = device_factory;
	this->rascsi_image = rascsi_image;

	log_levels.push_back("trace");
	log_levels.push_back("debug");
	log_levels.push_back("info");
	log_levels.push_back("warn");
	log_levels.push_back("err");
	log_levels.push_back("critical");
	log_levels.push_back("off");
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

PbImageFilesInfo *RascsiResponse::GetAvailableImages(PbResult& result)
{
	PbImageFilesInfo *image_files_info = new PbImageFilesInfo();

	string default_image_folder = rascsi_image->GetDefaultImageFolder();
	image_files_info->set_default_image_folder(default_image_folder);

	// filesystem::directory_iterator cannot be used because libstdc++ 8.3.0 does not support big files
	DIR *d = opendir(default_image_folder.c_str());
	if (d) {
		struct dirent *dir;
		while ((dir = readdir(d))) {
			if (dir->d_type == DT_REG || dir->d_type == DT_LNK || dir->d_type == DT_BLK) {
				string filename = default_image_folder + "/" + dir->d_name;

				struct stat st;
				if (dir->d_type == DT_REG && !stat(filename.c_str(), &st)) {
					if (!st.st_size) {
						LOGTRACE("File '%s' in image folder '%s' has a size of 0 bytes", dir->d_name, default_image_folder.c_str());
						continue;
					}
				} else if (dir->d_type == DT_LNK && stat(filename.c_str(), &st)) {
					LOGTRACE("Symlink '%s' in image folder '%s' is broken", dir->d_name, default_image_folder.c_str());
					continue;
				}

				PbImageFile *image_file = new PbImageFile();
				if (GetImageFile(image_file, dir->d_name)) {
					GetImageFile(image_files_info->add_image_files(), dir->d_name);
				}
				delete image_file;
			}
		}

	    closedir(d);
	}

	result.set_status(true);

	return image_files_info;
}

void RascsiResponse::GetAvailableImages(PbResult& result, PbServerInfo& server_info)
{
	PbImageFilesInfo *image_files_info = GetAvailableImages(result);
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
				ostringstream error;
				error << "No device for ID " << device.id() << ", unit " << device.unit();
				result.set_status(false);
				result.set_msg(error.str());
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
		const string& current_log_level)
{
	PbServerInfo *server_info = new PbServerInfo();

	server_info->set_allocated_version_info(GetVersionInfo(result));
	server_info->set_allocated_log_level_info(GetLogLevelInfo(result, current_log_level));
	GetAllDeviceTypeProperties(*server_info->mutable_device_types_info());
	GetAvailableImages(result, *server_info);
	server_info->set_allocated_network_interfaces_info(GetNetworkInterfacesInfo(result));
	server_info->set_allocated_mapping_info(GetMappingInfo(result));
	GetDevices(*server_info, devices);
	server_info->set_allocated_reserved_ids_info(GetReservedIds(result, reserved_ids));

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
