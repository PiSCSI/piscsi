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

TEST(PiscsiServiceTest, Init)
{
	PiscsiService service;

	EXPECT_FALSE(service.Init(nullptr, 65536)) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, 0)) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, -1)) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, 1)) << "Port 1 is only available for the root user";
	EXPECT_TRUE(service.Init(nullptr, 9999));
}

TEST(PiscsiServiceTest, StartStop)
{
	PiscsiService service;

	EXPECT_TRUE(service.Init(nullptr, 9999));

	service.Start();
	service.Stop();
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
	service.Init([] (const CommandContext& context) { PbResult result; context.WriteResult(result); return true; }, 9999);
	service.Start();
	EXPECT_TRUE(connect(fd, (sockaddr *)&server_addr, sizeof(server_addr)) >= 0) << "Service should be running";

	ASSERT_EQ(6, write(fd, "RASCSI", 6));

	PbCommand command;
	PbResult result;
	const ProtobufSerializer serializer;
	serializer.SerializeMessage(fd, command);
    serializer.DeserializeMessage(fd, result);

    close(fd);

    EXPECT_FALSE(result.status());

    service.Stop();
}
