//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Helper methods for serializing/deserializing protobuf messages
//
//---------------------------------------------------------------------------

#pragma once

#include "google/protobuf/message.h"
#include "rascsi_interface.pb.h"

void SerializeMessage(int, const google::protobuf::Message&);
void DeserializeMessage(int, google::protobuf::Message&);
int ReadNBytes(int, uint8_t *, int);
