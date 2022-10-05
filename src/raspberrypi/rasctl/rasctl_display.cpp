//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
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

void RasctlDisplay::DisplayDevices(const PbDevicesInfo& devices_info) const
{
	const list<PbDevice>& devices = { devices_info.devices().begin(), devices_info.devices().end() };

	cout << ListDevices(devices) << endl;
}

void RasctlDisplay::DisplayDeviceInfo(const PbDevice& pb_device) const
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

	DisplayParams(pb_device);

	cout << endl;
}

void RasctlDisplay::DisplayVersionInfo(const PbVersionInfo& version_info) const
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

void RasctlDisplay::DisplayLogLevelInfo(const PbLogLevelInfo& log_level_info) const
{
	if (!log_level_info.log_levels_size()) {
		cout << "  No log level settings available\n";
	}
	else {
		cout << "rascsi log levels, sorted by severity:\n";

		for (const auto& log_level : log_level_info.log_levels()) {
			cout << "  " << log_level << '\n';
		}
	}

	cout << "Current rascsi log level: " << log_level_info.current_log_level() << endl;
}

void RasctlDisplay::DisplayDeviceTypesInfo(const PbDeviceTypesInfo& device_types_info) const
{
	cout << "Supported device types and their properties:";

	for (const auto& device_type_info : device_types_info.properties()) {
		cout << "\n  " << PbDeviceType_Name(device_type_info.type()) << "  ";

		const PbDeviceProperties& properties = device_type_info.properties();

		DisplayAttributes(properties);

		if (properties.supports_file()) {
			cout << "Image file support\n        ";
		}

		if (properties.supports_params()) {
			cout << "Parameter support\n        ";
		}

		DisplayDefaultParameters(properties);

		DisplayBlockSizes(properties);
	}

	cout << flush;
}

void RasctlDisplay::DisplayReservedIdsInfo(const PbReservedIdsInfo& reserved_ids_info) const
{
	if (reserved_ids_info.ids_size()) {
		cout << "Reserved device IDs: ";

		for (int i = 0; i < reserved_ids_info.ids_size(); i++) {
			if(i) {
				cout << ", ";
			}

			cout << reserved_ids_info.ids(i);
		}

		cout << endl;
	}
}

void RasctlDisplay::DisplayImageFile(const PbImageFile& image_file_info) const
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

void RasctlDisplay::DisplayImageFiles(const PbImageFilesInfo& image_files_info) const
{
	cout << "Default image file folder: " << image_files_info.default_image_folder() << '\n';
	cout << "Supported folder depth: " << image_files_info.depth() << '\n';

	if (image_files_info.image_files().empty()) {
		cout << "  No image files available\n";
	}
	else {
		list<PbImageFile> image_files = { image_files_info.image_files().begin(), image_files_info.image_files().end() };
		image_files.sort([](const auto& a, const auto& b) { return a.name() < b.name(); });

		cout << "Available image files:\n";
		for (const auto& image_file : image_files) {
			cout << "  ";

			DisplayImageFile(image_file);
		}
	}

	cout << flush;
}

void RasctlDisplay::DisplayNetworkInterfaces(const PbNetworkInterfacesInfo& network_interfaces_info) const
{
	cout << "Available (up) network interfaces:\n";

	const list<string> sorted_interfaces = { network_interfaces_info.name().begin(), network_interfaces_info.name().end() };

	bool isFirst = true;
	for (const auto& interface : sorted_interfaces) {
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

void RasctlDisplay::DisplayMappingInfo(const PbMappingInfo& mapping_info) const
{
	cout << "Supported image file extension to device type mappings:\n";

	const map<string, PbDeviceType, less<>> sorted_mappings = { mapping_info.mapping().begin(), mapping_info.mapping().end() };

	for (const auto& [extension, type] : sorted_mappings) {
		cout << "  " << extension << "->" << PbDeviceType_Name(type) << '\n';
	}

	cout << flush;
}

void RasctlDisplay::DisplayOperationInfo(const PbOperationInfo& operation_info) const
{
	const map<int, PbOperationMetaData, less<>> operations = { operation_info.operations().begin(), operation_info.operations().end() };

	// Copies result into a map sorted by operation name
	auto unknown_operation = make_unique<PbOperationMetaData>();
	map<string, PbOperationMetaData, less<>> sorted_operations;

	for (const auto& [ordinal, meta_data] : operations) {
		if (PbOperation_IsValid(static_cast<PbOperation>(ordinal))) {
			sorted_operations[PbOperation_Name(static_cast<PbOperation>(ordinal))] = meta_data;
		}
		else {
			// If the server-side operation is unknown for the client use the server-provided operation name
			// No further operation information is available in this case
			sorted_operations[meta_data.server_side_name()] = *unknown_operation;
		}
	}

	cout << "Operations supported by rascsi server and their parameters:\n";
	for (const auto& [name, meta_data] : sorted_operations) {
		if (!meta_data.server_side_name().empty()) {
			cout << "  " << name;
			if (!meta_data.description().empty()) {
				cout << " (" << meta_data.description() << ")";
			}
			cout << '\n';

			DisplayParameters(meta_data);
		}
		else {
			cout << "  " << name << " (Unknown server-side operation)\n";
		}
	}

	cout << flush;
}

void RasctlDisplay::DisplayParams(const PbDevice& pb_device) const
{
	const map<string, string, less<>> sorted_params = { pb_device.params().begin(), pb_device.params().end() };

	bool isFirst = true;
	for (const auto& [key, value] : sorted_params) {
		if (!isFirst) {
			cout << ":";
		}

		isFirst = false;
		cout << key << "=" << value;
	}
}

void RasctlDisplay::DisplayAttributes(const PbDeviceProperties& properties) const
{
	if (properties.read_only() || properties.protectable() || properties.stoppable() || properties.lockable()) {
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

		cout << "\n        ";
	}
}

void RasctlDisplay::DisplayDefaultParameters(const PbDeviceProperties& properties) const
{
	if (properties.supports_params() && properties.default_params_size()) {
		cout << "Default parameters: ";

		const map<string, string, less<>> sorted_params = { properties.default_params().begin(), properties.default_params().end() };

		bool isFirst = true;
		for (const auto& [key, value] : sorted_params) {
			if (!isFirst) {
				cout << "\n                            ";
			}
			cout << key << "=" << value;

			isFirst = false;
		}
	}

}

void RasctlDisplay::DisplayBlockSizes(const PbDeviceProperties& properties) const
{
	if (properties.block_sizes_size()) {
		cout << "Configurable block sizes in bytes: ";

		const set<uint32_t> sorted_sizes = { properties.block_sizes().begin(), properties.block_sizes().end() };

		bool isFirst = true;
		for (const auto& size : sorted_sizes) {
			if (!isFirst) {
				cout << ", ";
			}
			cout << size;

			isFirst = false;
		}
	}
}

void RasctlDisplay::DisplayParameters(const PbOperationMetaData& meta_data) const
{
	list<PbOperationParameter> sorted_parameters = { meta_data.parameters().begin(), meta_data.parameters().end() };
	sorted_parameters.sort([](const auto& a, const auto& b) { return a.name() < b.name(); });

	for (const auto& parameter : sorted_parameters) {
		cout << "    " << parameter.name() << ": "
			<< (parameter.is_mandatory() ? "mandatory" : "optional");

		if (!parameter.description().empty()) {
			cout << " (" << parameter.description() << ")";
		}
		cout << '\n';

		DisplayPermittedValues(parameter);

		if (!parameter.default_value().empty()) {
			cout << "      Default value: " << parameter.default_value() << '\n';
		}
	}
}

void RasctlDisplay::DisplayPermittedValues(const PbOperationParameter& parameter) const
{
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

		cout << '\n';
	}
}
