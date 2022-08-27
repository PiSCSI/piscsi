//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rascsi_interface.pb.h"
#include "rasutil.h"
#include "rasctl_display.h"
#include <iostream>
#include <list>

using namespace std;
using namespace rascsi_interface;
using namespace ras_util;

void RasctlDisplay::DisplayDevices(const PbDevicesInfo& devices_info)
{
	const list<PbDevice>& devices = { devices_info.devices().begin(), devices_info.devices().end() };
	cout << ListDevices(devices) << endl;
}

void RasctlDisplay::DisplayDeviceInfo(const PbDevice& pb_device)
{
	cout << "  " << pb_device.id() << ":" << pb_device.unit() << "  " << PbDeviceType_Name(pb_device.type())
			<< "  " << pb_device.vendor() << ":" << pb_device.product() << ":" << pb_device.revision();

	if (pb_device.block_size()) {
		cout << "  " << pb_device.block_size() << " bytes per sector";
		if (pb_device.block_count()) {
			cout << "  " << pb_device.block_size() * pb_device.block_count() << " bytes capacity";
		}
	}

	if (pb_device.properties().supports_file() && !pb_device.file().name().empty()) {
		cout << "  " << pb_device.file().name();
	}

	cout << "  ";
	bool hasProperty = false;
	if (pb_device.properties().read_only()) {
		cout << "read-only";
		hasProperty = true;
	}
	if (pb_device.properties().protectable() && pb_device.status().protected_()) {
		if (hasProperty) {
			cout << ", ";
		}
		cout << "protected";
		hasProperty = true;
	}
	if (pb_device.properties().stoppable() && pb_device.status().stopped()) {
		if (hasProperty) {
			cout << ", ";
		}
		cout << "stopped";
		hasProperty = true;
	}
	if (pb_device.properties().removable() && pb_device.status().removed()) {
		if (hasProperty) {
			cout << ", ";
		}
		cout << "removed";
		hasProperty = true;
	}
	if (pb_device.properties().lockable() && pb_device.status().locked()) {
		if (hasProperty) {
			cout << ", ";
		}
		cout << "locked";
	}
	if (hasProperty) {
		cout << "  ";
	}

	// Creates a sorted map
	map<string, string> params = { pb_device.params().begin(), pb_device.params().end() };
	bool isFirst = true;
	for (const auto& param : params) {
		if (!isFirst) {
			cout << ":";
		}
		isFirst = false;
		cout << param.first << "=" << param.second;
	}

	cout << endl;
}

void RasctlDisplay::DisplayVersionInfo(const PbVersionInfo& version_info)
{
	cout << "rascsi server version: " << version_info.major_version() << "." << version_info.minor_version();
	if (version_info.patch_version() > 0) {
		cout << "." << version_info.patch_version();
	}
	else if (version_info.patch_version() < 0) {
		cout << " (development version)";
	}
	cout << endl;
}

void RasctlDisplay::DisplayLogLevelInfo(const PbLogLevelInfo& log_level_info)
{
	if (!log_level_info.log_levels_size()) {
		cout << "  No log level settings available" << endl;
	}
	else {
		cout << "rascsi log levels, sorted by severity:" << endl;
		for (const auto& log_level : log_level_info.log_levels()) {
			cout << "  " << log_level << endl;
		}
	}

	cout << "Current rascsi log level: " << log_level_info.current_log_level() << endl;
}

void RasctlDisplay::DisplayDeviceTypesInfo(const PbDeviceTypesInfo& device_types_info)
{
	cout << "Supported device types and their properties:";
	for (const auto& device_type_info : device_types_info.properties()) {
		cout << endl << "  " << PbDeviceType_Name(device_type_info.type()) << "  ";

		const PbDeviceProperties& properties = device_type_info.properties();

		if (properties.read_only() || properties.protectable() || properties.stoppable() || properties.read_only()
				|| properties.lockable()) {
			cout << "Properties: ";
			bool has_property = false;
			if (properties.read_only()) {
				cout << "read-only";
				has_property = true;
			}
			if (properties.protectable()) {
				cout << (has_property ? ", " : "") << "protectable";
				has_property = true;
			}
			if (properties.stoppable()) {
				cout << (has_property ? ", " : "") << "stoppable";
				has_property = true;
			}
			if (properties.removable()) {
				cout << (has_property ? ", " : "") << "removable";
				has_property = true;
			}
			if (properties.lockable()) {
				cout << (has_property ? ", " : "") << "lockable";
			}
			cout << endl << "        ";
		}

		if (properties.supports_file()) {
			cout << "Image file support" << endl << "        ";
		}
		if (properties.supports_params()) {
			cout << "Parameter support" << endl << "        ";
		}

		if (properties.supports_params() && properties.default_params_size()) {
			// Creates a sorted map
			map<string, string> params = { properties.default_params().begin(), properties.default_params().end() };

			cout << "Default parameters: ";

			bool isFirst = true;
			for (const auto& param : params) {
				if (!isFirst) {
					cout << endl << "                            ";
				}
				cout << param.first << "=" << param.second;

				isFirst = false;
			}
		}

		if (properties.block_sizes_size()) {
			// Creates a sorted set
			set<uint32_t> block_sizes = { properties.block_sizes().begin(), properties.block_sizes().end() };

			cout << "Configurable block sizes in bytes: ";

			bool isFirst = true;
			for (const auto& block_size : block_sizes) {
				if (!isFirst) {
					cout << ", ";
				}
				cout << block_size;

				isFirst = false;
			}
		}
	}
}

void RasctlDisplay::DisplayReservedIdsInfo(const PbReservedIdsInfo& reserved_ids_info)
{
	if (reserved_ids_info.ids_size()) {
		cout << "Reserved device IDs: ";
		for (int i = 0; i < reserved_ids_info.ids_size(); i++) {
			if(i) {
				cout << ", ";
			}
			cout << reserved_ids_info.ids(i);
		}
		cout <<endl;
	}
}

void RasctlDisplay::DisplayImageFile(const PbImageFile& image_file_info)
{
	cout << image_file_info.name() << "  " << image_file_info.size() << " bytes";
	if (image_file_info.read_only()) {
		cout << "  read-only";
	}
	if (image_file_info.type() != UNDEFINED) {
		cout << "  " << PbDeviceType_Name(image_file_info.type());
	}
	cout << endl;

}

void RasctlDisplay::DisplayImageFiles(const PbImageFilesInfo& image_files_info)
{
	cout << "Default image file folder: " << image_files_info.default_image_folder() << endl;
	cout << "Supported folder depth: " << image_files_info.depth() << endl;

	if (image_files_info.image_files().empty()) {
		cout << "  No image files available" << endl;
	}
	else {
		list<PbImageFile> image_files = { image_files_info.image_files().begin(), image_files_info.image_files().end() };
		image_files.sort([](const auto& a, const auto& b) { return a.name() < b.name(); });

		cout << "Available image files:" << endl;
		for (const auto& image_file : image_files) {
			cout << "  ";
			DisplayImageFile(image_file);
		}
	}
}

void RasctlDisplay::DisplayNetworkInterfaces(const PbNetworkInterfacesInfo& network_interfaces_info)
{
	// Creates a sorted list
	const list<string> interfaces = { network_interfaces_info.name().begin(), network_interfaces_info.name().end() };

	cout << "Available (up) network interfaces:" << endl;
	bool isFirst = true;
	for (const auto& interface : interfaces) {
		if (!isFirst) {
			cout << ", ";
		}
		else {
			cout << "  ";
		}
		isFirst = false;
		cout << interface;
	}
	cout << endl;
}

void RasctlDisplay::DisplayMappingInfo(const PbMappingInfo& mapping_info)
{
	// Creates a sorted map
	const map<string, PbDeviceType> mappings = { mapping_info.mapping().begin(), mapping_info.mapping().end() };

	cout << "Supported image file extension to device type mappings:" << endl;
	for (const auto&  mapping : mappings) {
		cout << "  " << mapping.first << "->" << PbDeviceType_Name(mapping.second) << endl;
	}
}

void RasctlDisplay::DisplayOperationInfo(const PbOperationInfo& operation_info)
{
	const map<int, PbOperationMetaData> operations = { operation_info.operations().begin(), operation_info.operations().end() };

	// Copies result into a map sorted by operation name
	const PbOperationMetaData *unknown_operation = new PbOperationMetaData();
	map<string, PbOperationMetaData> sorted_operations;
	for (const auto& operation : operations) {
		if (PbOperation_IsValid(static_cast<PbOperation>(operation.first))) {
			sorted_operations[PbOperation_Name(static_cast<PbOperation>(operation.first))] = operation.second;
		}
		else {
			// If the server-side operation is unknown for the client use the server-provided operation name
			// No further operation information is available in this case
			sorted_operations[operation.second.server_side_name()] = *unknown_operation;
		}
	}

	cout << "Operations supported by rascsi server and their parameters:" << endl;
	for (const auto& operation : sorted_operations) {
		if (!operation.second.server_side_name().empty()) {
			cout << "  " << operation.first;
			if (!operation.second.description().empty()) {
				cout << " (" << operation.second.description() << ")";
			}
			cout << endl;

			list<PbOperationParameter> sorted_parameters = { operation.second.parameters().begin(), operation.second.parameters().end() };
			sorted_parameters.sort([](const auto& a, const auto& b) { return a.name() < b.name(); });

			for (const auto& parameter : sorted_parameters) {
				cout << "    " << parameter.name() << ": "
					<< (parameter.is_mandatory() ? "mandatory" : "optional");
				if (!parameter.description().empty()) {
					cout << " (" << parameter.description() << ")";
				}
				cout << endl;

				if (parameter.permitted_values_size()) {
					cout << "      Permitted values: ";
					bool isFirst = true;
					for (const auto& permitted_value : parameter.permitted_values()) {
						if (!isFirst) {
							cout << ", ";
						}
						isFirst = false;
						cout << permitted_value;
					}
					cout << endl;
				}

				if (!parameter.default_value().empty()) {
					cout << "      Default value: " << parameter.default_value() << endl;
				}
			}
		}
		else {
			cout << "  " << operation.first << " (Unknown server-side operation)" << endl;
		}
	}
}
