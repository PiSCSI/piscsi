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

namespace {

int GetAvailablePort()
{
	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		return 0;
	}

	sockaddr_in server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
#ifdef __APPLE__
	server_addr.sin_len = sizeof(server_addr);
#endif
	if (::bind(fd, reinterpret_cast<const sockaddr *>(&server_addr), sizeof(server_addr)) == -1) {
		close(fd);
		return 0;
	}

	socklen_t addr_size = sizeof(server_addr);
	if (getsockname(fd, reinterpret_cast<sockaddr *>(&server_addr), &addr_size) == -1) {
		close(fd);
		return 0;
	}

	close(fd);
	return ntohs(server_addr.sin_port);
}

bool SendCommand(const PbCommand& command, PbResult& result, int port)
{
	sockaddr_in server_addr = {};
	if (!ResolveHostName("127.0.0.1", &server_addr)) {
		return false;
	}
	server_addr.sin_port = htons(static_cast<uint16_t>(port));

	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		return false;
	}

	if (connect(fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0) { //NOSONAR bit_cast is not supported by the bullseye clang++ compiler
		close(fd);
		return false;
	}

	if (write(fd, "RASCSI", 6) != 6) {
		close(fd);
		return false;
	}

	SerializeMessage(fd, command);
	DeserializeMessage(fd, result);
	close(fd);

	return true;
}

}

TEST(PiscsiServiceTest, Init)
{
	PiscsiService service;
	const int port = GetAvailablePort();
	if (!port) {
		GTEST_SKIP() << "Unable to bind an ephemeral TCP port";
	}

	EXPECT_FALSE(service.Init(nullptr, 65536).empty()) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, 0).empty()) << "Illegal port number";
	EXPECT_FALSE(service.Init(nullptr, -1).empty()) << "Illegal port number";
	EXPECT_TRUE(service.Init(nullptr, port).empty()) << "Selected port is expected not to be in use for this test";
	service.Stop();
}

TEST(PiscsiServiceTest, IsRunning)
{
	PiscsiService service;
	const int port = GetAvailablePort();
	if (!port) {
		GTEST_SKIP() << "Unable to bind an ephemeral TCP port";
	}
	EXPECT_FALSE(service.IsRunning());
	EXPECT_TRUE(service.Init(nullptr, port).empty()) << "Selected port is expected not to be in use for this test";
	EXPECT_FALSE(service.IsRunning());

	service.Start();
	EXPECT_TRUE(service.IsRunning());
	service.Stop();
	EXPECT_FALSE(service.IsRunning());
}

TEST(PiscsiServiceTest, Execute)
{
	const int port = GetAvailablePort();
	if (!port) {
		GTEST_SKIP() << "Unable to bind an ephemeral TCP port";
	}

	sockaddr_in server_addr = {};
	ASSERT_TRUE(ResolveHostName("127.0.0.1", &server_addr));

	const int fd = socket(AF_INET, SOCK_STREAM, 0);
	ASSERT_NE(-1, fd);

	server_addr.sin_port = htons(static_cast<uint16_t>(port));
	EXPECT_FALSE(connect(fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) >= 0) << "Service should not be running"; //NOSONAR bit_cast is not supported by the bullseye clang++ compiler

	close(fd);

	PiscsiService service;
	const string init_error = service.Init([] (const CommandContext& context) {
		if (context.GetCommand().operation() == PbOperation::NO_OPERATION) {
			PbResult result;
			result.set_status(true);
			context.WriteResult(result);
		}
		else {
			throw io_exception("error");
		}
		return true;
	}, port);
	ASSERT_TRUE(init_error.empty()) << init_error;

	service.Start();

	PbCommand command;
	PbResult result;

	command.set_operation(PbOperation::NO_OPERATION);
	ASSERT_TRUE(SendCommand(command, result, port));
	EXPECT_TRUE(result.status()) << "Command should have been successful";

	command.set_operation(PbOperation::EJECT);
	ASSERT_TRUE(SendCommand(command, result, port));
	EXPECT_FALSE(result.status()) << "Exception should have been raised";

	service.Stop();
}
