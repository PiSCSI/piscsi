//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Helper for serializing/deserializing protobuf messages
//
//---------------------------------------------------------------------------

#pragma once

#include "google/protobuf/message.h"
#include <vector>

using namespace std;

class ProtobufSerializer
{
public:

	ProtobufSerializer() = default;
	~ProtobufSerializer() = default;

	void SerializeMessage(int, const google::protobuf::Message&) const;
	void DeserializeMessage(int, google::protobuf::Message&) const;
	size_t ReadBytes(int, vector<byte>&) const;
};
