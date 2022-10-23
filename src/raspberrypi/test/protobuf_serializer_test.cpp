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

	const int fd = open("/dev/null", O_WRONLY);
	EXPECT_NE(-1, fd);
	serializer.SerializeMessage(fd, result);
	EXPECT_THROW(serializer.SerializeMessage(-1, result), io_exception) << "Writing a message must fail";
	close(fd);
}

TEST(ProtobufSerializerTest, DeserializeMessage)
{
	PbResult result;
	ProtobufSerializer serializer;
	vector<byte> buf(1);

	int fd = open("/dev/null", O_RDONLY);
	EXPECT_NE(-1, fd);
	EXPECT_THROW(serializer.DeserializeMessage(fd, result), io_exception) << "Reading the message header must fail";
	close(fd);

	string filename;
	fd = OpenTempFile(filename);
	EXPECT_NE(-1, fd);
	// Data size -1
	buf = { byte{0xff}, byte{0xff}, byte{0xff}, byte{0xff} };
	EXPECT_EQ(buf.size(), write(fd, buf.data(), buf.size()));
	close(fd);
	fd = open(filename.c_str(), O_RDONLY);
	EXPECT_NE(-1, fd);
	EXPECT_THROW(serializer.DeserializeMessage(fd, result), io_exception) << "Invalid header was not rejected";
	unlink(filename.c_str());

	fd = OpenTempFile(filename);
	EXPECT_NE(-1, fd);
	// Data size 2
	buf = { byte{0x02}, byte{0x00}, byte{0x00}, byte{0x00} };
	EXPECT_EQ(buf.size(), write(fd, buf.data(), buf.size()));
	close(fd);
	fd = open(filename.c_str(), O_RDONLY);
	EXPECT_NE(-1, fd);
	EXPECT_THROW(serializer.DeserializeMessage(fd, result), io_exception) << "Invalid data were not rejected";
	unlink(filename.c_str());
}

TEST(ProtobufSerializerTest, SerializeDeserializeMessage)
{
	PbResult result;
	result.set_status(true);
	ProtobufSerializer serializer;

	string filename;
	int fd = OpenTempFile(filename);
	EXPECT_NE(-1, fd);
	serializer.SerializeMessage(fd, result);
	close(fd);

	result.set_status(false);
	fd = open(filename.c_str(), O_RDONLY);
	EXPECT_NE(-1, fd);
	serializer.DeserializeMessage(fd, result);
	close(fd);
	unlink(filename.c_str());

	EXPECT_TRUE(result.status());
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
