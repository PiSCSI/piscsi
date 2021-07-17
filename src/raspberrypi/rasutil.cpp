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

#include <string.h>
#include "rasutil.h"

// Serialize protobuf data: Length followed by the actual protobuf data
void SerializeProtobufData(FILE* fp, const std::string& data)
{
    int len = data.length();
    fwrite(&len, sizeof(len), 1, fp);
    void* buf = malloc(data.length());
    memcpy(buf, data.data(), data.length());
    fwrite(buf, data.length(), 1, fp);
    free(buf);
}
