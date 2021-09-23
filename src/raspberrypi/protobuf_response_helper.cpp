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
#include "protobuf_response_helper.h"
#include <sstream>

using namespace rascsi_interface;

ProtobufResponseHandler::ProtobufResponseHandler()
{
	device_factory = DeviceFactory::instance();

	log_levels.push_back("trace");
	log_levels.push_back("debug");
	log_levels.push_back("info");
	log_levels.push_back("warn");
	log_levels.push_back("err");
	log_levels.push_back("critical");
	log_levels.push_back("off");
}

ProtobufResponseHandler& ProtobufResponseHandler::instance()
{
	static ProtobufResponseHandler instance;
	return instance;
}

PbDeviceProperties *ProtobufResponseHandler::GetDeviceProperties(const Device *device)
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
		for (const auto& param : device_factory.GetDefaultParams(t)) {
			auto& map = *properties->mutable_default_params();
			map[param.first] = param.second;
		}
	}

	for (const auto& block_size : device_factory.GetSectorSizes(t)) {
		properties->add_block_sizes(block_size);
	}

	for (const auto& capacity : device_factory.GetCapacities(t)) {
		properties->add_capacities(capacity);
	}

	return properties;
}

void ProtobufResponseHandler::GetDeviceTypeProperties(PbDeviceTypesInfo& device_types_info, PbDeviceType type)
{
	PbDeviceTypeProperties *type_properties = device_types_info.add_properties();
	type_properties->set_type(type);
	Device *device = device_factory.CreateDevice(type, "");
	type_properties->set_allocated_properties(GetDeviceProperties(device));
	delete device;
}

void ProtobufResponseHandler::GetAllDeviceTypeProperties(PbDeviceTypesInfo& device_types_info)
{
	GetDeviceTypeProperties(device_types_info, SAHD);
	GetDeviceTypeProperties(device_types_info, SCHD);
	GetDeviceTypeProperties(device_types_info, SCRM);
	GetDeviceTypeProperties(device_types_info, SCMO);
	GetDeviceTypeProperties(device_types_info, SCCD);
	GetDeviceTypeProperties(device_types_info, SCBR);
	GetDeviceTypeProperties(device_types_info, SCDP);
}

void ProtobufResponseHandler::GetDevice(const Device *device, PbDevice *pb_device, const string& image_folder)
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
		GetImageFile(image_file, device->IsRemovable() && !device->IsReady() ? "" : filepath.GetPath(), image_folder);
		pb_device->set_allocated_file(image_file);
	}
}

void ProtobufResponseHandler::GetImageFile(PbImageFile *image_file, const string& filename, const string& image_folder)
{
	image_file->set_name(filename);
	if (!filename.empty()) {
		image_file->set_type(DeviceFactory::GetTypeForFile(filename));

		string f = filename[0] == '/' ? filename : image_folder + "/" + filename;

		image_file->set_read_only(access(f.c_str(), W_OK));

		struct stat st;
		if (!stat(f.c_str(), &st)) {
			image_file->set_size(st.st_size);
		}
	}
}

PbImageFilesInfo *ProtobufResponseHandler::GetAvailableImages(const string& image_folder)
{
	PbImageFilesInfo *image_files_info = new PbImageFilesInfo();

	image_files_info->set_default_image_folder(image_folder);

	// filesystem::directory_iterator cannot be used because libstdc++ 8.3.0 does not support big files
	DIR *d = opendir(image_folder.c_str());
	if (d) {
		struct dirent *dir;
		while ((dir = readdir(d))) {
			if (dir->d_type == DT_REG || dir->d_type == DT_LNK || dir->d_type == DT_BLK) {
				string filename = image_folder + "/" + dir->d_name;

				struct stat st;
				if (dir->d_type == DT_REG && !stat(filename.c_str(), &st)) {
					if (!st.st_size) {
						LOGTRACE("File '%s' in image folder '%s' has a size of 0 bytes", dir->d_name, image_folder.c_str());
						continue;
					}

					if (st.st_size % 512) {
						LOGTRACE("Size of file '%s' in image folder '%s' is not a multiple of 512", dir->d_name, image_folder.c_str());
						continue;
					}
				} else if (dir->d_type == DT_LNK && stat(filename.c_str(), &st)) {
					LOGTRACE("Symlink '%s' in image folder '%s' is broken", dir->d_name, image_folder.c_str());
					continue;
				}

				GetImageFile(image_files_info->add_image_files(), dir->d_name, image_folder);
			}
		}

	    closedir(d);
	}

	return image_files_info;
}

void ProtobufResponseHandler::GetAvailableImages(PbServerInfo& server_info, const string& image_folder)
{
	PbImageFilesInfo *image_files_info = GetAvailableImages(image_folder);
	image_files_info->set_default_image_folder(image_folder);
	server_info.set_allocated_image_files_info(image_files_info);
}

void ProtobufResponseHandler::GetDevices(PbServerInfo& serverInfo, const vector<Device *>& devices, const string& image_folder)
{
	for (const Device *device : devices) {
		// Skip if unit does not exist or is not assigned
		if (device) {
			PbDevice *pb_device = serverInfo.mutable_devices()->add_devices();
			GetDevice(device, pb_device, image_folder);
		}
	}
}

void ProtobufResponseHandler::GetDevicesInfo(PbResult& result, const PbCommand& command, const vector<Device *>& devices,
		const string& image_folder, int unit_count)
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

	PbDevices *pb_devices = new PbDevices();
	result.set_allocated_device_info(pb_devices);

	for (const auto& id_set : id_sets) {
		const Device *device = devices[id_set.first * unit_count + id_set.second];
		GetDevice(device, pb_devices->add_devices(), image_folder);
	}

	result.set_status(true);
}

void ProtobufResponseHandler::GetDeviceTypesInfo(PbResult& result, const PbCommand& command)
{
	PbDeviceTypesInfo *device_types_info = new PbDeviceTypesInfo();
	result.set_allocated_device_types_info(device_types_info);

	GetAllDeviceTypeProperties(*device_types_info);

	result.set_status(true);
}

void ProtobufResponseHandler::GetServerInfo(PbResult& result, const vector<Device *>& devices, const set<int>& reserved_ids,
		const string& image_folder, const string& log_level)
{
	PbServerInfo *server_info = new PbServerInfo();
	result.set_allocated_server_info(server_info);

	server_info->set_major_version(rascsi_major_version);
	server_info->set_minor_version(rascsi_minor_version);
	server_info->set_patch_version(rascsi_patch_version);
	GetLogLevels(*server_info);
	server_info->set_current_log_level(log_level);
	GetAllDeviceTypeProperties(*server_info->mutable_device_types_info());
	GetAvailableImages(*server_info, image_folder);
	server_info->set_allocated_network_interfaces_info(GetNetworkInterfacesInfo());
	GetDevices(*server_info, devices, image_folder);
	for (int id : reserved_ids) {
		server_info->add_reserved_ids(id);
	}

	result.set_status(true);
}

void ProtobufResponseHandler::GetLogLevels(PbServerInfo& server_info)
{
	for (const auto& log_level : log_levels) {
		server_info.add_log_levels(log_level);
	}
}

PbNetworkInterfacesInfo *ProtobufResponseHandler::GetNetworkInterfacesInfo()
{
	PbNetworkInterfacesInfo *network_interfaces_info = new PbNetworkInterfacesInfo();

	for (const auto& network_interface : device_factory.GetNetworkInterfaces()) {
		network_interfaces_info->add_name(network_interface);
	}

	return network_interfaces_info;
}
