//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Helper for serializing/deserializing protobuf messages
//
//---------------------------------------------------------------------------

#pragma once

#include "google/protobuf/message.h"
#include "rascsi_interface.pb.h"
#include "command_context.h"
#include "localizer.h"
#include <string>

using namespace std;
using namespace rascsi_interface;

class ProtobufConnector
{
public:

	ProtobufConnector() = default;
	~ProtobufConnector() = default;

	int ReadCommand(PbCommand&, int);
	void SerializeMessage(int, const google::protobuf::Message&);
	void DeserializeMessage(int, google::protobuf::Message&);
	size_t ReadBytes(int, vector<byte>&);
};
