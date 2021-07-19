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

#if !defined(rasutil_h)
#define rasutil_h

#include <cstdio>
#include <string>

void SerializeProtobufData(FILE *fp, const std::string& data);
std::string DeserializeProtobufData(int fd);

#endif
