//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2020 akuker
//
//---------------------------------------------------------------------------

#include <unistd.h>
#include <sstream>
#include "rascsi_interface.pb.h"
#include "exceptions.h"
#include "rasutil.h"

using namespace std;
using namespace rascsi_interface;

//---------------------------------------------------------------------------
//
//	Serialize/Deserialize protobuf message: Length followed by the actual data.
//  Little endian is assumed.
//
//---------------------------------------------------------------------------

void SerializeMessage(int fd, const google::protobuf::MessageLite& message)
{
	string data;
	message.SerializeToString(&data);

	// Write the size of the protobuf data as a header
    int32_t size = data.length();
    if (write(fd, &size, sizeof(size)) != sizeof(size)) {
    	throw ioexception("Can't write protobuf header");
    }

    // Write the actual protobuf data
    uint8_t buf[size];
    memcpy(buf, data.data(), size);
    if (write(fd, buf, size) != size) {
    	throw ioexception("Can't write protobuf data");
    }
}

void DeserializeMessage(int fd, google::protobuf::MessageLite& message)
{
	// Read the header with the size of the protobuf data
	uint8_t header_buf[4];
	int bytes_read = ReadNBytes(fd, header_buf, 4);
	if (bytes_read < 4) {
		return;
	}
	int32_t size = (header_buf[3] << 24) + (header_buf[2] << 16) + (header_buf[1] << 8) + header_buf[0];

	// Read the binary protobuf data
	uint8_t data_buf[size];
	bytes_read = ReadNBytes(fd, data_buf, size);
	if (bytes_read < size) {
		throw ioexception("Missing protobuf data");
	}

	// Create protobuf message
	string data((const char *)data_buf, size);
	message.ParseFromString(data);
}

int ReadNBytes(int fd, uint8_t *buf, int n)
{
	int offset = 0;
	while (offset < n) {
		ssize_t len = read(fd, buf + offset, n - offset);
		if (!len) {
			break;
		}

		offset += len;
	}

	return offset;
}

//---------------------------------------------------------------------------
//
//	List devices
//
//---------------------------------------------------------------------------
string ListDevices(const PbDevices& devices)
{
	ostringstream s;

	if (devices.devices_size()) {
		s << "+----+----+------+-------------------------------------" << endl
			<< "| ID | UN | TYPE | DEVICE STATUS" << endl
			<< "+----+----+------+-------------------------------------" << endl;
	}
	else {
		return "No images currently attached.";
	}

	for (int i = 0; i < devices.devices_size() ; i++) {
		PbDevice device = devices.devices(i);

		string filename;
		switch (device.type()) {
			case SCBR:
				filename = "X68000 HOST BRIDGE";
				break;

			case SCDP:
				filename = "DaynaPort SCSI/Link";
				break;

			default:
				filename = device.image_file().name();
				break;
		}

		s << "|  " << device.id() << " |  " << device.unit() << " | " << PbDeviceType_Name(device.type()) << " | "
				<< (filename.empty() ? "NO MEDIA" : filename)
				<< (device.read_only() ? " (WRITEPROTECT)" : "") << endl;
	}

	s << "+----+----+------+-------------------------------------";

	return s.str();
}
