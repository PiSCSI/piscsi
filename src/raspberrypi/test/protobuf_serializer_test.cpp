//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "rascsi_exceptions.h"
#include "rascsi_interface.pb.h"
#include "protobuf_serializer.h"

using namespace rascsi_interface;

TEST(ProtobufSerializerTest, SerializeMessage)
{
	PbResult message;
	ProtobufSerializer serializer;

	int fd = open("/dev/null", O_WRONLY);
	ASSERT_NE(-1, fd);
	serializer.SerializeMessage(fd, message);
	close(fd);
	EXPECT_THROW(serializer.SerializeMessage(-1, message), io_exception);
}
