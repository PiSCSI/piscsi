//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Helper for serializing/deserializing protobuf messages to/fromn sockets
//
//---------------------------------------------------------------------------

#pragma once

#include "google/protobuf/message.h"
#include "rascsi_interface.pb.h"
#include "command_context.h"
#include "localizer.h"
#include <string>

using namespace rascsi_interface; //NOSONAR Not relevant for rascsi

class SocketConnector
{
public:

	SocketConnector() = default;
	~SocketConnector() = default;

	int ReadCommand(PbCommand&, int) const;
	void SerializeMessage(int, const google::protobuf::Message&) const;
	void DeserializeMessage(int, google::protobuf::Message&) const;
	size_t ReadBytes(int, vector<byte>&) const;
};
