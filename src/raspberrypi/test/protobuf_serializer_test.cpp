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
	PbResult result;
	ProtobufSerializer serializer;

	int fd = open("/dev/null", O_WRONLY);
	EXPECT_NE(-1, fd);
	serializer.SerializeMessage(fd, result);
	EXPECT_THROW(serializer.SerializeMessage(-1, result), io_exception);
	close(fd);
}

TEST(ProtobufSerializerTest, DeserializeMessage)
{
	PbResult result;
	ProtobufSerializer serializer;
	vector<byte> buf(1);

	int fd = open("/dev/zero", O_RDONLY);
	EXPECT_NE(-1, fd);
	EXPECT_THROW(serializer.DeserializeMessage(fd, result), io_exception);
	close(fd);
}

TEST(ProtobufSerializerTest, ReadBytes)
{
	ProtobufSerializer serializer;
	vector<byte> buf(1);

	int fd = open("/dev/null", O_RDONLY);
	EXPECT_NE(-1, fd);
	EXPECT_EQ(0, serializer.ReadBytes(fd, buf));
	close(fd);

	fd = open("/dev/zero", O_RDONLY);
	EXPECT_NE(-1, fd);
	EXPECT_EQ(1, serializer.ReadBytes(fd, buf));
	close(fd);
}
