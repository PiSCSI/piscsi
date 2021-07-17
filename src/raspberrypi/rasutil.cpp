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
    int len = data.length();
    fwrite(&len, sizeof(len), 1, fp);

    void *buf = malloc(data.length());
    memcpy(buf, data.data(), data.length());
    fwrite(buf, data.length(), 1, fp);

    free(buf);
}

string DeserializeProtobufData(int fd)
{
	FILE *fp = fdopen(fd, "r+");
	if (!fp) {
		throw ioexception();
	}
	setvbuf(fp, NULL, _IONBF, 0);

	int len;
	size_t res = fread(&len, sizeof(int), 1, fp);
	if (res != 1) {
		fclose(fp);

		throw ioexception();
	}

	void *buf = malloc(len);
	res = fread(buf, len, 1, fp);
	if (res != 1) {
		free(buf);

		fclose(fp);

		throw ioexception();
	}

	string data((const char *)buf, len);
	free(buf);

	fclose(fp);

	return data;
}
