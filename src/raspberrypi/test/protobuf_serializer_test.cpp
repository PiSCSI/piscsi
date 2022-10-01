//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "rascsi_exceptions.h"
#include "rascsi_interface.pb.h"
#include "protobuf_serializer.h"

using namespace rascsi_interface;

TEST(ProtobufSerializerTest, SerializeMessage)
{
	PbResult message;
	ProtobufSerializer serializer;

	serializer.SerializeMessage(STDOUT_FILENO, message);
	EXPECT_THROW(serializer.SerializeMessage(-1, message), io_exception);
}
