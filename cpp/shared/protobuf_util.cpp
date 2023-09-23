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

	for (const auto& p : Split(params)) {
		if (const size_t separator_pos = p.find(KEY_VALUE_SEPARATOR); separator_pos != string::npos) {
			SetParam(device, p.substr(0, separator_pos), string_view(p).substr(separator_pos + 1));
		}
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

		s << "|  " << device.id() << " |   " << device.unit() << " | " << PbDeviceType_Name(device.type()) << " | "
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
