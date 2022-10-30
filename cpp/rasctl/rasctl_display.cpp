//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "list_devices.h"
#include "rascsi_interface.pb.h"
#include "rasutil.h"
#include "rasctl_display.h"
#include <sstream>
#include <list>
#include <iomanip>

using namespace std;
using namespace rascsi_interface;
using namespace ras_util;

string RasctlDisplay::DisplayDevicesInfo(const PbDevicesInfo& devices_info) const
{
	ostringstream s;

	const list<PbDevice>& devices = { devices_info.devices().begin(), devices_info.devices().end() };

	s << ListDevices(devices);

	return s.str();
}

string RasctlDisplay::DisplayDeviceInfo(const PbDevice& pb_device) const
{
	ostringstream s;

	s << "  " << pb_device.id() << ":" << pb_device.unit() << "  " << PbDeviceType_Name(pb_device.type())
			<< "  " << pb_device.vendor() << ":" << pb_device.product() << ":" << pb_device.revision();

	if (pb_device.block_size()) {
		s << "  " << pb_device.block_size() << " bytes per sector";

		if (pb_device.block_count()) {
			s << "  " << pb_device.block_size() * pb_device.block_count() << " bytes capacity";
		}
	}

	if (pb_device.properties().supports_file() && !pb_device.file().name().empty()) {
		s << "  " << pb_device.file().name();
	}

	s << "  ";

	bool hasProperty = false;

	if (pb_device.properties().read_only()) {
		s << "read-only";
		hasProperty = true;
	}

	if (pb_device.properties().protectable() && pb_device.status().protected_()) {
		if (hasProperty) {
			s << ", ";
		}
		s << "protected";
		hasProperty = true;
	}

	if (pb_device.properties().stoppable() && pb_device.status().stopped()) {
		if (hasProperty) {
			s << ", ";
		}
		s << "stopped";
		hasProperty = true;
	}

	if (pb_device.properties().removable() && pb_device.status().removed()) {
		if (hasProperty) {
			s << ", ";
		}
		s << "removed";
		hasProperty = true;
	}

	if (pb_device.properties().lockable() && pb_device.status().locked()) {
		if (hasProperty) {
			s << ", ";
		}
		s << "locked";
	}

	if (hasProperty) {
		s << "  ";
	}

	DisplayParams(s, pb_device);

	s << '\n';

	return s.str();
}

string RasctlDisplay::DisplayVersionInfo(const PbVersionInfo& version_info) const
{
	ostringstream s;

	s << "rascsi server version: " << setw(2) << setfill('0') << version_info.major_version() << "."
			<< setw(2) << setfill('0') << version_info.minor_version();

	if (version_info.patch_version() > 0) {
		s << "." << version_info.patch_version();
	}
	else if (version_info.patch_version() < 0) {
		s << " (development version)";
	}

	s << '\n';

	return s.str();
}

string RasctlDisplay::DisplayLogLevelInfo(const PbLogLevelInfo& log_level_info) const
{
	ostringstream s;

	if (!log_level_info.log_levels_size()) {
		s << "  No log level settings available\n";
	}
	else {
		s << "rascsi log levels, sorted by severity:\n";

		for (const auto& log_level : log_level_info.log_levels()) {
			s << "  " << log_level << '\n';
		}
	}

	s << "Current rascsi log level: " << log_level_info.current_log_level() << '\n';

	return s.str();
}

string RasctlDisplay::DisplayDeviceTypesInfo(const PbDeviceTypesInfo& device_types_info) const
{
	ostringstream s;

	s << "Supported device types and their properties:";

	for (const auto& device_type_info : device_types_info.properties()) {
		s << "\n  " << PbDeviceType_Name(device_type_info.type()) << "  ";

		const PbDeviceProperties& properties = device_type_info.properties();

		DisplayAttributes(s, properties);

		if (properties.supports_file()) {
			s << "Image file support\n        ";
		}

		if (properties.supports_params()) {
			s << "Parameter support\n        ";
		}

		DisplayDefaultParameters(s, properties);

		DisplayBlockSizes(s, properties);
	}

	s << '\n';

	return s.str();
}

string RasctlDisplay::DisplayReservedIdsInfo(const PbReservedIdsInfo& reserved_ids_info) const
{
	ostringstream s;

	if (reserved_ids_info.ids_size()) {
		s << "Reserved device IDs: ";

		for (int i = 0; i < reserved_ids_info.ids_size(); i++) {
			if(i) {
				s << ", ";
			}

			s << reserved_ids_info.ids(i);
		}

		s << '\n';
	}
	else {
		s << "No reserved device IDs\n";
	}

	return s.str();
}

string RasctlDisplay::DisplayImageFile(const PbImageFile& image_file_info) const
{
	ostringstream s;

	s << image_file_info.name() << "  " << image_file_info.size() << " bytes";

	if (image_file_info.read_only()) {
		s << "  read-only";
	}

	if (image_file_info.type() != UNDEFINED) {
		s << "  " << PbDeviceType_Name(image_file_info.type());
	}

	s << '\n';

	return s.str();
}

string RasctlDisplay::DisplayImageFilesInfo(const PbImageFilesInfo& image_files_info) const
{
	ostringstream s;

	s << "Default image file folder: " << image_files_info.default_image_folder() << '\n';
	s << "Supported folder depth: " << image_files_info.depth() << '\n';

	if (image_files_info.image_files().empty()) {
		s << "  No image files available\n";
	}
	else {
		list<PbImageFile> image_files = { image_files_info.image_files().begin(), image_files_info.image_files().end() };
		image_files.sort([](const auto& a, const auto& b) { return a.name() < b.name(); });

		s << "Available image files:\n";
		for (const auto& image_file : image_files) {
			s << "  ";

			s << DisplayImageFile(image_file);
		}
	}

	return s.str();
}

string RasctlDisplay::DisplayNetworkInterfaces(const PbNetworkInterfacesInfo& network_interfaces_info) const
{
	ostringstream s;

	s << "Available (up) network interfaces:\n";

	const list<string> sorted_interfaces = { network_interfaces_info.name().begin(), network_interfaces_info.name().end() };

	bool isFirst = true;
	for (const auto& interface : sorted_interfaces) {
		if (!isFirst) {
			s << ", ";
		}
		else {
			s << "  ";
		}

		isFirst = false;
		s << interface;
	}

	s << '\n';

	return s.str();
}

string RasctlDisplay::DisplayMappingInfo(const PbMappingInfo& mapping_info) const
{
	ostringstream s;

	s << "Supported image file extension to device type mappings:\n";

	const map<string, PbDeviceType, less<>> sorted_mappings = { mapping_info.mapping().begin(), mapping_info.mapping().end() };

	for (const auto& [extension, type] : sorted_mappings) {
		s << "  " << extension << "->" << PbDeviceType_Name(type) << '\n';
	}

	return s.str();
}

string RasctlDisplay::DisplayOperationInfo(const PbOperationInfo& operation_info) const
{
	ostringstream s;

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

	s << "Operations supported by rascsi server and their parameters:\n";
	for (const auto& [name, meta_data] : sorted_operations) {
		if (!meta_data.server_side_name().empty()) {
			s << "  " << name;
			if (!meta_data.description().empty()) {
				s << " (" << meta_data.description() << ")";
			}
			s << '\n';

			DisplayParameters(s, meta_data);
		}
		else {
			s << "  " << name << " (Unknown server-side operation)\n";
		}
	}

	return s.str();
}

void RasctlDisplay::DisplayParams(ostringstream& s, const PbDevice& pb_device) const
{
	const map<string, string, less<>> sorted_params = { pb_device.params().begin(), pb_device.params().end() };

	bool isFirst = true;
	for (const auto& [key, value] : sorted_params) {
		if (!isFirst) {
			s << ":";
		}

		isFirst = false;
		s << key << "=" << value;
	}
}

void RasctlDisplay::DisplayAttributes(ostringstream& s, const PbDeviceProperties& properties) const
{
	if (properties.read_only() || properties.protectable() || properties.stoppable() || properties.lockable()) {
		s << "Properties: ";

		bool has_property = false;

		if (properties.read_only()) {
			s << "read-only";
			has_property = true;
		}

		if (properties.protectable()) {
			s << (has_property ? ", " : "") << "protectable";
			has_property = true;
		}
		if (properties.stoppable()) {
			s << (has_property ? ", " : "") << "stoppable";
			has_property = true;
		}
		if (properties.removable()) {
			s << (has_property ? ", " : "") << "removable";
			has_property = true;
		}
		if (properties.lockable()) {
			s << (has_property ? ", " : "") << "lockable";
		}

		s << "\n        ";
	}
}

void RasctlDisplay::DisplayDefaultParameters(ostringstream& s, const PbDeviceProperties& properties) const
{
	if (properties.supports_params() && properties.default_params_size()) {
		s << "Default parameters: ";

		const map<string, string, less<>> sorted_params = { properties.default_params().begin(), properties.default_params().end() };

		bool isFirst = true;
		for (const auto& [key, value] : sorted_params) {
			if (!isFirst) {
				s << "\n                            ";
			}
			s << key << "=" << value;

			isFirst = false;
		}
	}

}

void RasctlDisplay::DisplayBlockSizes(ostringstream& s, const PbDeviceProperties& properties) const
{
	if (properties.block_sizes_size()) {
		s << "Configurable block sizes in bytes: ";

		const set<uint32_t> sorted_sizes = { properties.block_sizes().begin(), properties.block_sizes().end() };

		bool isFirst = true;
		for (const auto& size : sorted_sizes) {
			if (!isFirst) {
				s << ", ";
			}
			s << size;

			isFirst = false;
		}
	}
}

void RasctlDisplay::DisplayParameters(ostringstream& s, const PbOperationMetaData& meta_data) const
{
	list<PbOperationParameter> sorted_parameters = { meta_data.parameters().begin(), meta_data.parameters().end() };
	sorted_parameters.sort([](const auto& a, const auto& b) { return a.name() < b.name(); });

	for (const auto& parameter : sorted_parameters) {
		s << "    " << parameter.name() << ": "
			<< (parameter.is_mandatory() ? "mandatory" : "optional");

		if (!parameter.description().empty()) {
			s << " (" << parameter.description() << ")";
		}
		s << '\n';

		DisplayPermittedValues(s, parameter);

		if (!parameter.default_value().empty()) {
			s << "      Default value: " << parameter.default_value() << '\n';
		}
	}
}

void RasctlDisplay::DisplayPermittedValues(ostringstream& s, const PbOperationParameter& parameter) const
{
	if (parameter.permitted_values_size()) {
		s << "      Permitted values: ";

		bool isFirst = true;

		for (const auto& permitted_value : parameter.permitted_values()) {
			if (!isFirst) {
				s << ", ";
			}

			isFirst = false;
			s << permitted_value;
		}

		s << '\n';
	}
}
