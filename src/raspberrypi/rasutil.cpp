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

#include <cstring>
#include <unistd.h>
#include "exceptions.h"
#include "rasutil.h"

using namespace std;

//---------------------------------------------------------------------------
//
//	Serialize/Deserialize protobuf data: Length followed by the actual data
//
//---------------------------------------------------------------------------

void SerializeProtobufData(int fd, const string& data)
{
	// Write the size of the protobuf data as a header
    ssize_t size = data.length();
    if (write(fd, &size, sizeof(size)) != sizeof(size)) {
    	throw ioexception("Cannot write protobuf header");
    }

    // Write the actual protobuf data
    void *buf = malloc(size);
    memcpy(buf, data.data(), size);
    if (write(fd, buf, size) != size) {
    	free(buf);

    	throw ioexception("Cannot write protobuf data");
    }

    free(buf);
}

string DeserializeProtobufData(int fd)
{
	// First read the header with the size of the protobuf data
	size_t size;
	if (read(fd, &size, sizeof(int)) != sizeof(int)) {
		// No more data
		return "";
	}

	// Read the actual protobuf data
	void *buf = malloc(size);
	if (read(fd, buf, size) != (ssize_t)size) {
		free(buf);

		throw ioexception("Missing protobuf data");
	}

	// Read protobuf data into a string, to be converted into a protobuf data structure by the caller
	string data((const char *)buf, size);
	free(buf);

	return data;
}
