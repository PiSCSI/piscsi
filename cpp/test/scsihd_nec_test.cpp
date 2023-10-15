//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/piscsi_exceptions.h"
#include "devices/scsihd_nec.h"
#include <filesystem>
#include <fstream>

using namespace std;
using namespace filesystem;

void ScsiHdNecTest_SetUpModePages(map<int, vector<byte>>& pages)
{
	EXPECT_EQ(5, pages.size()) << "Unexpected number of mode pages";
	EXPECT_EQ(12, pages[1].size());
	EXPECT_EQ(24, pages[3].size());
	EXPECT_EQ(20, pages[4].size());
	EXPECT_EQ(12, pages[8].size());
	EXPECT_EQ(30, pages[48].size());
}

TEST(ScsiHdNecTest, Inquiry)
{
	TestInquiry::Inquiry(SCHD, device_type::direct_access, scsi_level::scsi_1_ccs, "PiSCSI                  ", 0x1f, false, "file.hdn");
}

TEST(ScsiHdNecTest, SetUpModePages)
{
	map<int, vector<byte>> pages;
	MockSCSIHD_NEC hd(0);

	// Non changeable
	hd.SetUpModePages(pages, 0x3f, false);
	ScsiHdNecTest_SetUpModePages(pages);

	// Changeable
	pages.clear();
	hd.SetUpModePages(pages, 0x3f, true);
	ScsiHdNecTest_SetUpModePages(pages);
}

TEST(ScsiHdNecTest, TestAddErrorPage)
{
	map<int, vector<byte>> pages;
	MockSCSIHD_NEC hd(0);

	hd.SetBlockCount(0x1234);
	hd.SetReady(true);
	// Non changeable
	hd.SetUpModePages(pages, 0x01, false);
	EXPECT_EQ(1, pages.size()) << "Unexpected number of mode pages";
	const vector<byte>& page_1 = pages[1];
	EXPECT_EQ(0x26, to_integer<int>(page_1[2]));
}

TEST(ScsiHdNecTest, TestAddFormatPage)
{
	map<int, vector<byte>> pages;
	MockSCSIHD_NEC hd(0);

	hd.SetBlockCount(0x1234);
	hd.SetReady(true);
	hd.SetUpModePages(pages, 0x03, false);
	EXPECT_EQ(1, pages.size()) << "Unexpected number of mode pages";
	vector<byte>& page_3 = pages[3];
	EXPECT_EQ(0x80, to_integer<int>(page_3[0]) & 0x80);
	EXPECT_EQ(0, to_integer<int>(page_3[20]));

	hd.SetRemovable(true);
	// Non changeable
	hd.SetUpModePages(pages, 0x03, false);
	page_3 = pages[3];
	EXPECT_EQ(0x20, to_integer<int>(page_3[20]));

	pages.clear();
	// Changeable
	hd.SetUpModePages(pages, 0x03, true);
	EXPECT_EQ(0xffff, GetInt16(page_3, 12));
}

TEST(ScsiHdNecTest, TestAddDrivePage)
{
	map<int, vector<byte>> pages;
	MockSCSIHD_NEC hd(0);

	hd.SetBlockCount(0x1234);
	hd.SetReady(true);
	// Non changeable
	hd.SetUpModePages(pages, 0x04, false);
	EXPECT_EQ(1, pages.size()) << "Unexpected number of mode pages";

	pages.clear();
	// Changeable
	hd.SetUpModePages(pages, 0x04, true);
	EXPECT_EQ(1, pages.size()) << "Unexpected number of mode pages";
}

TEST(ScsiHdNecTest, SetParameters)
{
	MockSCSIHD_NEC hd_hdn(0);
	MockSCSIHD_NEC hd_hdi(0);
	MockSCSIHD_NEC hd_nhd(0);

	EXPECT_THROW(hd_hdn.Open(), io_exception) << "Missing filename";

	path tmp = CreateTempFile(511);
	hd_hdn.SetFilename(string(tmp));
	EXPECT_THROW(hd_hdn.Open(), io_exception) << "Root sector file is too small";
	remove(tmp);

	tmp = CreateTempFile(512);
	hd_hdn.SetFilename(string(tmp));
	EXPECT_THROW(hd_hdn.Open(), io_exception) << "Invalid file extension";

	const auto hdn = path((string)tmp + ".HDN");
	rename(tmp, hdn);
	hd_hdn.SetFilename(string(hdn));
	hd_hdn.Open();
	remove(hdn);

	tmp = CreateTempFile(512);
	const auto hdi = path((string)tmp + ".hdi");
	rename(tmp, hdi);
	hd_hdi.SetFilename(string(hdi));
	EXPECT_THROW(hd_hdi.Open(), io_exception) << "Invalid sector size";

	ofstream out;
	out.open(hdi);
	const array<const char, 4> cylinders1 = { 1, 0, 0, 0 };
	out.seekp(28);
	out.write(cylinders1.data(), cylinders1.size());
	const array<const char, 4> heads1 = { 1, 0, 0, 0 };
	out.seekp(24);
	out.write(heads1.data(), heads1.size());
	const array<const char, 4> sectors1 = { 1, 0, 0, 0 };
	out.seekp(20);
	out.write(sectors1.data(), sectors1.size());
	const array<const char, 4>  sector_size1 = { 0, 2, 0, 0 };
	out.seekp(16);
	out.write(sector_size1.data(), sector_size1.size());
	const array<const char, 4> image_size = { 0, 2, 0, 0 };
	out.seekp(12);
	out.write(image_size.data(), image_size.size());
	out.close();
	resize_file(hdi, 512);
	hd_hdi.Open();

	remove(hdi);

	tmp = CreateTempFile(512);
	const auto nhd = path((string)tmp + ".nhd");
	rename(tmp, nhd);
	hd_nhd.SetFilename(string(nhd));
	EXPECT_THROW(hd_nhd.Open(), io_exception) << "Invalid file format";

	out.open(nhd);
	out << "T98HDDIMAGE.R0";
	out.close();
	resize_file(nhd, 512);
	EXPECT_THROW(hd_nhd.Open(), io_exception) << "Invalid sector size";

	out.open(nhd);
	out << "T98HDDIMAGE.R0";
	// 512 bytes per sector
	array<char, 2> sector_size2 = { 0, 2 };
	out.seekp(0x11c);
	out.write(sector_size2.data(), sector_size2.size());
	out.close();
	resize_file(nhd, 512);
	EXPECT_THROW(hd_nhd.Open(), io_exception) << "Drive has 0 blocks";

	out.open(nhd);
	out << "T98HDDIMAGE.R0";
	const array<const char, 2> cylinders2 = { 1, 0 };
	out.seekp(0x114);
	out.write(cylinders2.data(), cylinders2.size());
	const array<const char, 2> heads2 = { 1, 0 };
	out.seekp(0x118);
	out.write(heads2.data(), heads2.size());
	const array<const char, 2> sectors2 = { 1, 0 };
	out.seekp(0x11a);
	out.write(sectors2.data(), sectors2.size());
	out.seekp(0x11c);
	out.write(sector_size2.data(), sector_size2.size());
	const array<const char, 4> image_offset = { 1, 0, 0, 0 };
	out.seekp(0x110);
	out.write(image_offset.data(), image_offset.size());
	out.close();
	resize_file(nhd, 512);
	EXPECT_THROW(hd_nhd.Open(), io_exception) << "Invalid image offset/size";

	out.open(nhd);
	out << "T98HDDIMAGE.R0";
	out.seekp(0x114);
	out.write(cylinders2.data(), cylinders2.size());
	out.seekp(0x118);
	out.write(heads2.data(), heads2.size());
	out.seekp(0x11a);
	out.write(sectors2.data(), sectors2.size());
	// 1 byte per sector
	sector_size2 = { 1, 0 };
	out.seekp(0x11c);
	out.write(sector_size2.data(), sector_size2.size());
	out.close();
	resize_file(nhd, 512);
	EXPECT_THROW(hd_nhd.Open(), io_exception) << "Invalid NEC disk/sector size";

	out.open(nhd);
	out << "T98HDDIMAGE.R0";
	out.seekp(0x114);
	out.write(cylinders2.data(), cylinders2.size());
	out.seekp(0x118);
	out.write(heads2.data(), heads2.size());
	out.seekp(0x11a);
	out.write(sectors2.data(), sectors2.size());
	sector_size2 = { 0, 2 };
	out.seekp(0x11c);
	out.write(sector_size2.data(), sector_size2.size());
	out.close();
	resize_file(nhd, 512);
	hd_nhd.Open();

	remove(nhd);
}
