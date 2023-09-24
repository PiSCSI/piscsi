//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "shared/piscsi_util.h"
#ifdef __linux__
#include <sched.h>
#endif

using namespace std;
using namespace piscsi_util;

TEST(PiscsiUtilTest, Split)
{
	auto v = Split("this_is_a_test", '_');
	EXPECT_EQ(4, v.size());
	EXPECT_EQ("this", v[0]);
	EXPECT_EQ("is", v[1]);
	EXPECT_EQ("a", v[2]);
	EXPECT_EQ("test", v[3]);
	v = Split("test", ':');
	EXPECT_EQ(1, v.size());
	EXPECT_EQ("test", v[0]);
	v = Split(":test", ':');
	EXPECT_EQ(2, v.size());
	EXPECT_EQ("", v[0]);
	EXPECT_EQ("test", v[1]);
	v = Split("test:", ':');
	EXPECT_EQ(1, v.size());
	EXPECT_EQ("test", v[0]);
	v = Split(":", ':');
	EXPECT_EQ(1, v.size());
	EXPECT_EQ("", v[0]);
	v = Split("", ':');
	EXPECT_EQ(0, v.size());

	v = Split("this:is:a:test", ':', 1);
	EXPECT_EQ(1, v.size());
	EXPECT_EQ("this:is:a:test", v[0]);
	v = Split("this:is:a:test", ':', 2);
	EXPECT_EQ(2, v.size());
	EXPECT_EQ("this", v[0]);
	EXPECT_EQ("is:a:test", v[1]);
}

TEST(PiscsiUtilTest, GetLocale)
{
	EXPECT_LE(2, GetLocale().size());
}

TEST(PiscsiUtilTest, ProcessId)
{
	int id = -1;
	int lun = -1;

	string error = ProcessId("", id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("8", id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("0:32", id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("-1:", id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("0:-1", id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("a", id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("a:0", id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("0:a", id, lun);
	EXPECT_FALSE(error.empty());
	EXPECT_EQ(-1, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("0", id, lun);
	EXPECT_TRUE(error.empty());
	EXPECT_EQ(0, id);
	EXPECT_EQ(-1, lun);

	error = ProcessId("7:31", id, lun);
	EXPECT_TRUE(error.empty());
	EXPECT_EQ(7, id);
	EXPECT_EQ(31, lun);
}

TEST(PiscsiUtilTest, GetAsUnsignedInt)
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

TEST(PiscsiUtilTest, Banner)
{
	EXPECT_FALSE(Banner("Test").empty());
}

TEST(PiscsiUtilTest, GetExtensionLowerCase)
{
	EXPECT_EQ("", GetExtensionLowerCase(""));
	EXPECT_EQ("", GetExtensionLowerCase("."));
	EXPECT_EQ("", GetExtensionLowerCase(".ext"));
	EXPECT_EQ("", GetExtensionLowerCase(".ext_long"));
	EXPECT_EQ("ext", GetExtensionLowerCase("file.ext"));
	EXPECT_EQ("ext", GetExtensionLowerCase("FILE.EXT"));
	EXPECT_EQ("ext", GetExtensionLowerCase(".XYZ.EXT"));
}

#ifdef __linux__
TEST(PiscsiUtilTest, FixCpu)
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
