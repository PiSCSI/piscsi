//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "piscsi_util.h"
#include "protobuf_util.h"
#include <unistd.h>
#include <sstream>
#include <array>
#include <vector>
#include <iomanip>

using namespace std;
using namespace piscsi_util;
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

	for (const auto& p : Split(params, COMPONENT_SEPARATOR)) {
		if (const auto& param = Split(p, KEY_VALUE_SEPARATOR, 2); param.size() == 2) {
			SetParam(device, param[0], param[1]);
		}
	}
}

string protobuf_util::SetCommandParams(PbCommand& command, const string& params)
{
	if (params.find(KEY_VALUE_SEPARATOR) != string::npos) {
		return SetFromGenericParams(command, params);
	}

	string folder_pattern;
	string file_pattern;
	string operations;

	switch (const auto& components = Split(params, COMPONENT_SEPARATOR, 3); components.size()) {
		case 3:
			operations = components[2];
			[[fallthrough]];

		case 2:
			folder_pattern = components[0];
			file_pattern = components[1];
			break;

		case 1:
			file_pattern = components[0];
			break;

		default:
			break;
	}

	SetParam(command, "folder_pattern", folder_pattern);
	SetParam(command, "file_pattern", file_pattern);
	SetParam(command, "operations", operations);

	return "";
}

string protobuf_util::SetFromGenericParams(PbCommand& command, const string& params)
{
	for (const string& key_value : Split(params, COMPONENT_SEPARATOR)) {
		const auto& param = Split(key_value, KEY_VALUE_SEPARATOR, 2);
		if (param.size() > 1 && !param[0].empty()) {
			SetParam(command, param[0], param[1]);
		}
		else {
			return "Parameter '" + key_value + "' has to be a key/value pair";
		}
	}

	return "";
}

void protobuf_util::SetProductData(PbDeviceDefinition& device, const string& data)
{
	const auto& components = Split(data, COMPONENT_SEPARATOR, 3);
	switch (components.size()) {
		case 3:
			device.set_revision(components[2]);
			[[fallthrough]];

		case 2:
			device.set_product(components[1]);
			[[fallthrough]];

		case 1:
			device.set_vendor(components[0]);
			break;

		default:
			break;
	}
}

string protobuf_util::SetIdAndLun(PbDeviceDefinition& device, const string& value)
{
	int id;
	int lun;
	if (const string error = ProcessId(value, id, lun); !error.empty()) {
		return error;
	}

	device.set_id(id);
	device.set_unit(lun != -1 ? lun : 0);

	return "";
}

string protobuf_util::ListDevices(const vector<PbDevice>& pb_devices)
{
	if (pb_devices.empty()) {
		return "No devices currently attached.\n";
	}

	ostringstream s;
	s << "+----+-----+------+-------------------------------------\n"
			<< "| ID | LUN | TYPE | IMAGE FILE\n"
			<< "+----+-----+------+-------------------------------------\n";

	vector<PbDevice> devices = pb_devices;
	ranges::sort(devices, [](const auto& a, const auto& b) { return a.id() < b.id() || a.unit() < b.unit(); });

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

		s << "|  " << device.id() << " | " << setw(3) << device.unit() << " | " << PbDeviceType_Name(device.type()) << " | "
				<< (filename.empty() ? "NO MEDIUM" : filename)
				<< (!device.status().removed() && (device.properties().read_only() || device.status().protected_()) ? " (READ-ONLY)" : "")
				<< '\n';
	}

	s << "+----+-----+------+-------------------------------------\n";

	return s.str();
}

//---------------------------------------------------------------------------
//
// Serialize/Deserialize protobuf message: Length followed by the actual data.
// A little endian platform is assumed.
//
//---------------------------------------------------------------------------

void protobuf_util::SerializeMessage(int fd, const google::protobuf::Message& message)
{
	const string data = message.SerializeAsString();

	// Write the size of the protobuf data as a header
	const auto size = static_cast<int32_t>(data.length());
    if (write(fd, &size, sizeof(size)) != sizeof(size)) {
    	throw io_exception("Can't write protobuf message size");
    }

    // Write the actual protobuf data
    if (write(fd, data.data(), size) != size) {
    	throw io_exception("Can't write protobuf message data");
    }
}

void protobuf_util::DeserializeMessage(int fd, google::protobuf::Message& message)
{
	// Read the header with the size of the protobuf data
	array<byte, sizeof(int32_t)> header_buf;
	if (ReadBytes(fd, header_buf) < header_buf.size()) {
		throw io_exception("Can't read protobuf message size");
	}

	const int size = (static_cast<int>(header_buf[3]) << 24) + (static_cast<int>(header_buf[2]) << 16)
			+ (static_cast<int>(header_buf[1]) << 8) + static_cast<int>(header_buf[0]);
	if (size < 0) {
		throw io_exception("Invalid protobuf message size");
	}

	// Read the binary protobuf data
	vector<byte> data_buf(size);
	if (ReadBytes(fd, data_buf) != data_buf.size()) {
		throw io_exception("Invalid protobuf message data");
	}

	message.ParseFromArray(data_buf.data(), size);
}

size_t protobuf_util::ReadBytes(int fd, span<byte> buf)
{
	size_t offset = 0;
	while (offset < buf.size()) {
		const auto len = read(fd, &buf.data()[offset], buf.size() - offset);
		if (len == -1) {
			throw io_exception("Read error: " + string(strerror(errno)));
		}

		if (!len) {
			break;
		}

		offset += len;
	}

	return offset;
}
