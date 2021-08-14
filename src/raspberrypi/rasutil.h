//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "google/protobuf/message_lite.h"
#include "rascsi_interface.pb.h"

void SerializeMessage(int, const google::protobuf::MessageLite&);
void DeserializeMessage(int, google::protobuf::MessageLite&);
int ReadNBytes(int, uint8_t *, int);
string ListDevices(const rascsi_interface::PbDevices&);
