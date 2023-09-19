//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// These tests only test up the point where a network connection is required.
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "generated/piscsi_interface.pb.h"
#include "shared/network_util.h"
#include "shared/protobuf_serializer.h"
#include "piscsi/command_context.h"
#include "piscsi/piscsi_service.h"
#include <unistd.h>
#include <netdb.h>

using namespace piscsi_interface;
using namespace network_util;

TEST(PiscsiServiceTest, InitServiceSocket)
{
	PiscsiService service;

	EXPECT_FALSE(service.InitServiceSocket(nullptr, 65536).empty()) << "Illegal port number";
	EXPECT_FALSE(service.InitServiceSocket(nullptr, 0).empty()) << "Illegal port number";
	EXPECT_FALSE(service.InitServiceSocket(nullptr, -1).empty()) << "Illegal port number";
	EXPECT_FALSE(service.InitServiceSocket(nullptr, 1).empty()) << "Port 1 is only available for the root user";
	EXPECT_TRUE(service.InitServiceSocket(nullptr, 9999).empty()) << "Port 9999 is expected not to be in use for this test";
	service.Stop();
}

TEST(PiscsiServiceTest, IsRunning)
{
	PiscsiService service;
	EXPECT_FALSE(service.IsRunning());
	EXPECT_TRUE(service.InitServiceSocket(nullptr, 9999).empty()) << "Port 9999 is expected not to be in use for this test";
	EXPECT_FALSE(service.IsRunning());

	service.Start();
	EXPECT_TRUE(service.IsRunning());
	service.Stop();
	EXPECT_FALSE(service.IsRunning());
}

TEST(PiscsiServiceTest, Execute)
{
	sockaddr_in server_addr = {};
	EXPECT_TRUE(ResolveHostName("127.0.0.1", &server_addr));

	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	ASSERT_NE(-1, fd);

	server_addr.sin_port = htons(uint16_t(9999));
	EXPECT_FALSE(connect(fd, (sockaddr *)&server_addr, sizeof(server_addr)) >= 0) << "Service should not be running";

	PiscsiService service;
	service.InitServiceSocket([] (const CommandContext& context) {
		PbResult result;
		result.set_status(true);
		context.WriteResult(result);
		return true;
	}, 9999);
	service.Start();
	EXPECT_TRUE(connect(fd, (sockaddr *)&server_addr, sizeof(server_addr)) >= 0) << "Service should be running";

	PbCommand command;
	PbResult result;
	const ProtobufSerializer serializer;
	ASSERT_EQ(6, write(fd, "RASCSI", 6));
	serializer.SerializeMessage(fd, command);
    serializer.DeserializeMessage(fd, result);
    close(fd);

    service.Stop();

    EXPECT_TRUE(result.status());
}
