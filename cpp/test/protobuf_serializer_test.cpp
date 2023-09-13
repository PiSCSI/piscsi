//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/protobuf_serializer.h"
#include "shared/piscsi_exceptions.h"
#include "generated/piscsi_interface.pb.h"
#include <filesystem>

using namespace filesystem;
using namespace piscsi_interface;

TEST(ProtobufSerializerTest, SerializeMessage)
{
	PbResult result;
	ProtobufSerializer serializer;

	const int fd = open("/dev/null", O_WRONLY);
	ASSERT_NE(-1, fd);
	serializer.SerializeMessage(fd, result);
	close(fd);
	EXPECT_THROW(serializer.SerializeMessage(-1, result), io_exception) << "Writing a message must fail";
}

TEST(ProtobufSerializerTest, DeserializeMessage)
{
	PbResult result;
	ProtobufSerializer serializer;
	vector<byte> buf(1);

	int fd = open("/dev/null", O_RDONLY);
	ASSERT_NE(-1, fd);
	EXPECT_THROW(serializer.DeserializeMessage(fd, result), io_exception) << "Reading the message header must fail";
	close(fd);

	auto [fd1, filename1] = OpenTempFile();
	// Data size -1
	buf = { byte{0xff}, byte{0xff}, byte{0xff}, byte{0xff} };
	EXPECT_EQ(buf.size(), write(fd1, buf.data(), buf.size()));
	close(fd1);
	fd1 = open(filename1.c_str(), O_RDONLY);
	ASSERT_NE(-1, fd1);
	EXPECT_THROW(serializer.DeserializeMessage(fd1, result), io_exception) << "Invalid header was not rejected";
	remove(filename1);

	auto [fd2, filename2] = OpenTempFile();
	// Data size 2
	buf = { byte{0x02}, byte{0x00}, byte{0x00}, byte{0x00} };
	EXPECT_EQ(buf.size(), write(fd2, buf.data(), buf.size()));
	close(fd2);
	fd2 = open(filename2.c_str(), O_RDONLY);
	EXPECT_NE(-1, fd2);
	EXPECT_THROW(serializer.DeserializeMessage(fd2, result), io_exception) << "Invalid data were not rejected";
	remove(filename2);
}

TEST(ProtobufSerializerTest, SerializeDeserializeMessage)
{
	PbResult result;
	result.set_status(true);
	ProtobufSerializer serializer;

	auto [fd, filename] = OpenTempFile();
	EXPECT_NE(-1, fd);
	serializer.SerializeMessage(fd, result);
	close(fd);

	result.set_status(false);
	fd = open(filename.c_str(), O_RDONLY);
	EXPECT_NE(-1, fd);
	serializer.DeserializeMessage(fd, result);
	close(fd);
	remove(filename);

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
