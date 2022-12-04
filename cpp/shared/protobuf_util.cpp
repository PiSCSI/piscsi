//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "pisutil.h"
#include "protobuf_serializer.h"
#include "protobuf_util.h"
#include <sstream>

using namespace std;
using namespace pis_util;
using namespace piscsi_interface;

#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )

void protobuf_util::ParseParameters(PbDeviceDefinition& device, const string& params)
{
	if (params.empty()) {
		return;
	}

	// Old style parameter (filename), for backwards compatibility only
	if (params.find(KEY_VALUE_SEPARATOR) == string::npos) {
		SetParam(device, "file", params);

		return;
	}

	stringstream ss(params);
	string p;
	while (getline(ss, p, COMPONENT_SEPARATOR)) {
		if (!p.empty()) {
			const size_t separator_pos = p.find(KEY_VALUE_SEPARATOR);
			if (separator_pos != string::npos) {
				SetParam(device, p.substr(0, separator_pos), string_view(p).substr(separator_pos + 1));
			}
		}
	}
}

string protobuf_util::GetParam(const PbCommand& command, const string& key)
{
	const auto& it = command.params().find(key);
	return it != command.params().end() ? it->second : "";
}

string protobuf_util::GetParam(const PbDeviceDefinition& device, const string& key)
{
	const auto& it = device.params().find(key);
	return it != device.params().end() ? it->second : "";
}

void protobuf_util::SetParam(PbCommand& command, const string& key, string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *command.mutable_params();
		map[key] = value;
	}
}

void protobuf_util::SetParam(PbDevice& device, const string& key, string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *device.mutable_params();
		map[key] = value;
	}
}

void protobuf_util::SetParam(PbDeviceDefinition& device, const string& key, string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *device.mutable_params();
		map[key] = value;
	}
}

void protobuf_util::SetPatternParams(PbCommand& command, string_view patterns)
{
	string folder_pattern;
	string file_pattern;
	if (const size_t separator_pos = patterns.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
		folder_pattern = patterns.substr(0, separator_pos);
		file_pattern = patterns.substr(separator_pos + 1);
	}
	else {
		file_pattern = patterns;
	}

	SetParam(command, "folder_pattern", folder_pattern);
	SetParam(command, "file_pattern", file_pattern);
}

void protobuf_util::SetProductData(PbDeviceDefinition& device, const string& data)
{
	string name = data;

	if (size_t separator_pos = name.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
		device.set_vendor(name.substr(0, separator_pos));
		name = name.substr(separator_pos + 1);
		separator_pos = name.find(COMPONENT_SEPARATOR);
		if (separator_pos != string::npos) {
			device.set_product(name.substr(0, separator_pos));
			device.set_revision(name.substr(separator_pos + 1));
		}
		else {
			device.set_product(name);
		}
	}
	else {
		device.set_vendor(name);
	}
}

string protobuf_util::SetIdAndLun(PbDeviceDefinition& device, const string& value, int max_luns)
{
	int id;
	int lun;
	if (const string error = ProcessId(value, max_luns, id, lun); !error.empty()) {
		return error;
	}

	device.set_id(id);
	device.set_unit(lun);

	return "";
}

string protobuf_util::ListDevices(const list<PbDevice>& pb_devices)
{
	if (pb_devices.empty()) {
		return "No devices currently attached.\n";
	}

	ostringstream s;
	s << "+----+-----+------+-------------------------------------\n"
			<< "| ID | LUN | TYPE | IMAGE FILE\n"
			<< "+----+-----+------+-------------------------------------\n";

	list<PbDevice> devices = pb_devices;
	devices.sort([](const auto& a, const auto& b) { return a.id() < b.id() || a.unit() < b.unit(); });

	for (const auto& device : devices) {
		string filename;
		switch (device.type()) {
			case SCBR:
				filename = "X68000 HOST BRIDGE";
				break;

			case SCDP:
				filename = "DaynaPort SCSI/Link";
				break;

			case SCHS:
				filename = "Host Services";
				break;

			case SCLP:
				filename = "SCSI Printer";
				break;

			default:
				filename = device.file().name();
				break;
		}

		s << "|  " << device.id() << " |   " << device.unit() << " | " << PbDeviceType_Name(device.type()) << " | "
				<< (filename.empty() ? "NO MEDIUM" : filename)
				<< (!device.status().removed() && (device.properties().read_only() || device.status().protected_()) ? " (READ-ONLY)" : "")
				<< '\n';
	}

	s << "+----+-----+------+-------------------------------------\n";

	return s.str();
}
