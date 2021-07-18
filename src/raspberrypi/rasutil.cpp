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
#include <cstdlib>
#include "exceptions.h"
#include "rasutil.h"

using namespace std;

//---------------------------------------------------------------------------
//
//	Serialize/Deserialize protobuf data: Length followed by the actual data
//
//---------------------------------------------------------------------------

void SerializeProtobufData(FILE *fp, const string& data)
{
	// Write the size of the protobuf data as a header
    size_t size = data.length();
    fwrite(&size, sizeof(size), 1, fp);

    // Write the actual protobuf data
    void *buf = malloc(size);
    memcpy(buf, data.data(), size);
    fwrite(buf, size, 1, fp);

    free(buf);
}

string DeserializeProtobufData(int fd)
{
	FILE *fp = fdopen(fd, "r+");
	if (!fp) {
		throw ioexception();
	}
	setvbuf(fp, NULL, _IONBF, 0);

	// First read the header with the size of the protobuf data
	int size;
	size_t res = fread(&size, sizeof(int), 1, fp);
	if (res != 1) {
		fclose(fp);

		throw ioexception();
	}

	// Read the actual protobuf data
	void *buf = malloc(size);
	res = fread(buf, size, 1, fp);
	if (res != 1) {
		free(buf);

		fclose(fp);

		throw ioexception();
	}

	// Read protobuf data into a string, to be converted into a protobuf data structure by the caller
	string data((const char *)buf, size);
	free(buf);

	fclose(fp);

	return data;
}
