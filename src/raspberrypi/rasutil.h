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

#include "google/protobuf/message_lite.h"
#include "rascsi_interface.pb.h"

void SerializeMessage(int, const google::protobuf::MessageLite&);
void DeserializeMessage(int, google::protobuf::MessageLite&);
int ReadNBytes(int, uint8_t *, int);
string ListDevices(const rascsi_interface::PbDevices&);

#endif
