//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/scsi_command_util.h"

using namespace scsi_command_util;

TEST(ScsiMoTest, Inquiry)
{
	TestInquiry(SCMO, device_type::OPTICAL_MEMORY, scsi_level::SCSI_2, scsi_level::SCSI_2,
			"RaSCSI  SCSI MO         ", 0x1f, true);
}

TEST(ScsiMoTest, SetUpModePages)
{
	map<int, vector<byte>> mode_pages;
	const unordered_set<uint32_t> sector_sizes;
	MockSCSIMO device(0, sector_sizes);

	device.SetUpModePages(mode_pages, 0x3f, false);
	EXPECT_EQ(6, mode_pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, mode_pages[1].size());
	EXPECT_EQ(24, mode_pages[3].size());
	EXPECT_EQ(24, mode_pages[4].size());
	EXPECT_EQ(4, mode_pages[6].size());
	EXPECT_EQ(12, mode_pages[8].size());
	EXPECT_EQ(12, mode_pages[32].size());
}

TEST(ScsiMoTest, ModeSense6)
{
	NiceMock<MockAbstractController> controller(0);
	const unordered_set<uint32_t> sector_sizes;
	auto mo = make_shared<MockSCSIMO>(0, sector_sizes);

	controller.AddDevice(mo);

	vector<int>& cmd = controller.GetCmd();

	cmd[2] = 0x3f;
	// ALLOCATION LENGTH
	cmd[4] = 255;
	EXPECT_TRUE(mo->Dispatch(scsi_command::eCmdModeSense6));
	EXPECT_EQ(100, controller.GetBuffer()[0]) << "Wrong data length";
}

TEST(ScsiMoTest, ModeSense10)
{
	NiceMock<MockAbstractController> controller(0);
	const unordered_set<uint32_t> sector_sizes;
	auto mo = make_shared<MockSCSIMO>(0, sector_sizes);

	controller.AddDevice(mo);

	vector<int>& cmd = controller.GetCmd();

	cmd[2] = 0x3f;
	// ALLOCATION LENGTH
	cmd[8] = 255;
	EXPECT_TRUE(mo->Dispatch(scsi_command::eCmdModeSense10));
	EXPECT_EQ(96, GetInt16(controller.GetBuffer(), 0)) << "Wrong data length";
}
