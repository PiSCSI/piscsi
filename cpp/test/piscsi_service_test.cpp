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
#include "shared/protobuf_util.h"
#include "shared/network_util.h"
#include "shared/piscsi_exceptions.h"
#include "piscsi/command_context.h"
#include "piscsi/piscsi_service.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

using namespace piscsi_interface;
using namespace protobuf_util;
using namespace network_util;

void SendCommand(const PbCommand& command, PbResult& result)
{
	sockaddr_in server_addr = {};
	ASSERT_TRUE(ResolveHostName("127.0.0.1", &server_addr));
	server_addr.sin_port = htons(uint16_t(9999));

	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	ASSERT_NE(-1, fd);
	EXPECT_TRUE(connect(fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) >= 0) << "Service should be running"; //NOSONAR bit_cast is not supported by the bullseye compiler
	ASSERT_EQ(6, write(fd, "RASCSI", 6));
	SerializeMessage(fd, command);
    DeserializeMessage(fd, result);
    close(fd);
}

TEST(PiscsiServiceTest, Init)
{
	PiscsiService service;

	EXPECT_FALSE(service.Init(nullptr, 65536).empty()) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, 0).empty()) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, -1).empty()) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, 1).empty()) << "Port 1 is only available for the root user";
	EXPECT_TRUE(service.Init(nullptr, 9999).empty()) << "Port 9999 is expected not to be in use for this test";
	service.Stop();
}

TEST(PiscsiServiceTest, IsRunning)
{
	PiscsiService service;
	EXPECT_FALSE(service.IsRunning());
	EXPECT_TRUE(service.Init(nullptr, 9999).empty()) << "Port 9999 is expected not to be in use for this test";
	EXPECT_FALSE(service.IsRunning());

	service.Start();
	EXPECT_TRUE(service.IsRunning());
	service.Stop();
	EXPECT_FALSE(service.IsRunning());
}

TEST(PiscsiServiceTest, Execute)
{
	sockaddr_in server_addr = {};
	ASSERT_TRUE(ResolveHostName("127.0.0.1", &server_addr));

	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	ASSERT_NE(-1, fd);

	server_addr.sin_port = htons(uint16_t(9999));
	EXPECT_FALSE(connect(fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) >= 0) << "Service should not be running"; //NOSONAR bit_cast is not supported by the bullseye compiler

	close(fd);

	PiscsiService service;
	service.Init([] (const CommandContext& context) {
		if (context.GetCommand().operation() == PbOperation::NO_OPERATION) {
			PbResult result;
			result.set_status(true);
			context.WriteResult(result);
		}
		else {
			throw io_exception("error");
		}
		return true;
	}, 9999);

	service.Start();

	PbCommand command;
	PbResult result;

	SendCommand(command, result);
	command.set_operation(PbOperation::NO_OPERATION);
    EXPECT_TRUE(result.status()) << "Command should have been successful";

    command.set_operation(PbOperation::EJECT);
    SendCommand(command, result);
    EXPECT_FALSE(result.status()) << "Exception should have been raised";

    service.Stop();
}
