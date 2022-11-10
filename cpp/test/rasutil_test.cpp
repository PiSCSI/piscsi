//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "shared/rasutil.h"
#include "generated/rascsi_interface.pb.h"
#ifdef __linux__
#include <sched.h>
#endif

using namespace std;
using namespace rascsi_interface;
using namespace ras_util;

TEST(RasUtilTest, ProcessId)
{
	int id = -1;
	int lun = -1;

	string error = ProcessId("", 32, id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("8", 0, id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("0:1", 0, id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("0:32", 32, id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("0", 32, id, lun);
	EXPECT_TRUE(error.empty());
	EXPECT_EQ(0, id);
	EXPECT_EQ(0, lun);

	error = ProcessId("7:31", 32, id, lun);
	EXPECT_TRUE(error.empty());
	EXPECT_EQ(7, id);
	EXPECT_EQ(31, lun);
}

TEST(RasUtilTest, GetAsUnsignedInt)
{
	int result;

	EXPECT_FALSE(GetAsUnsignedInt("", result));
	EXPECT_FALSE(GetAsUnsignedInt("xyz", result));
	EXPECT_FALSE(GetAsUnsignedInt("-1", result));
	EXPECT_FALSE(GetAsUnsignedInt("1234567898765432112345678987654321", result)) << "Value is out of range";
	EXPECT_TRUE(GetAsUnsignedInt("0", result));
	EXPECT_EQ(0, result);
	EXPECT_TRUE(GetAsUnsignedInt("1234", result));
	EXPECT_EQ(1234, result);
}

TEST(RasUtilTest, Banner)
{
	EXPECT_FALSE(Banner("Test").empty());
}

TEST(RasUtilTest, GetExtensionLowerCase)
{
	EXPECT_EQ("", GetExtensionLowerCase(""));
	EXPECT_EQ("", GetExtensionLowerCase("."));
	EXPECT_EQ("ext", GetExtensionLowerCase("file.ext"));
	EXPECT_EQ("ext", GetExtensionLowerCase("FILE.EXT"));
	EXPECT_EQ("ext", GetExtensionLowerCase(".ext"));
	EXPECT_EQ("ext_long", GetExtensionLowerCase(".ext_long"));
	EXPECT_EQ("ext", GetExtensionLowerCase(".XYZ.EXT"));
}

#ifdef __linux__
TEST(RasUtilTest, FixCpu)
{
	cpu_set_t mask;

	FixCpu(0);
	CPU_ZERO(&mask);
	const int cpu = sched_getaffinity(0, sizeof(cpu_set_t), &mask);
	const int cpus = CPU_COUNT(&mask);

	FixCpu(cpus);
	CPU_ZERO(&mask);
	EXPECT_EQ(cpu, sched_getaffinity(0, sizeof(cpu_set_t), &mask));

	FixCpu(cpus - 1);
	CPU_ZERO(&mask);
	EXPECT_EQ(cpus - 1, sched_getaffinity(0, sizeof(cpu_set_t), &mask));
	FixCpu(0);

	// Reset affinity
	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);

	EXPECT_EQ(0, cpu);
}
#endif
