//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2021-2023 Uwe Seimet
// Copyright (C) akuker
//
// Licensed under the BSD 3-Clause License.
// See LICENSE file in the project root folder.
//
//---------------------------------------------------------------------------

#include "shared/piscsi_util.h"
#include "shared/piscsi_exceptions.h"
#include "scsi_command_util.h"
#include "scsihd_nec.h"
#include <fstream>

using namespace piscsi_util;
using namespace scsi_command_util;

void SCSIHD_NEC::Open()
{
	assert(!IsReady());

	off_t size = GetFileSize();

	array<char, 512> root_sector;
	ifstream in(GetFilename(), ios::binary);
	in.read(root_sector.data(), root_sector.size());
	if (!in.good() || size < static_cast<off_t>(root_sector.size())) {
		throw io_exception("Can't read NEC hard disk file root sector");
	}

	// Effective size must be a multiple of 512
	size = (size / 512) * 512;

	// Determine parameters by extension
	const auto [image_size, sector_size] = SetParameters(root_sector, static_cast<int>(size));

	SetSectorSizeShiftCount(static_cast<uint32_t>(size));

	SetBlockCount(image_size >> GetSectorSizeShiftCount());

	FinalizeSetup(image_offset);
}

pair<int, int> SCSIHD_NEC::SetParameters(const array<char, 512>& data, int size)
{
	array<uint8_t, 512> root_sector = {};
	memcpy(root_sector.data(), data.data(), root_sector.size());

	int image_size;
	int sector_size;

	// PC-9801-55 NEC compatible?
	if (const string ext = GetExtensionLowerCase(GetFilename()); ext == "hdn") {
		// Assuming sector size 512, number of sectors 25, number of heads 8 as default settings
		image_offset = 0;
		image_size = size;
		sector_size = 512;
		sectors = 25;
		heads = 8;
		cylinders = size >> 9;
		cylinders >>= 3;
		cylinders /= 25;
	}
	// Anex86 HD image?
	else if (ext == "hdi") {
		image_offset = GetInt32LittleEndian(&root_sector[8]);
		image_size = GetInt32LittleEndian(&root_sector[12]);
		sector_size = GetInt32LittleEndian(&root_sector[16]);
		sectors = GetInt32LittleEndian(&root_sector[20]);
		heads = GetInt32LittleEndian(&root_sector[24]);
		cylinders = GetInt32LittleEndian(&root_sector[28]);
	}
	// T98Next HD image?
	else if (ext == "nhd") {
		if (!memcmp(root_sector.data(), "T98HDDIMAGE.R0\0", 15)) {
			image_offset = GetInt32LittleEndian(&root_sector[0x110]);
			cylinders = GetInt32LittleEndian(&root_sector[0x114]);
			heads = GetInt16LittleEndian(&root_sector[0x118]);
			sectors = GetInt16LittleEndian(&root_sector[0x11a]);
			sector_size = GetInt16LittleEndian(&root_sector[0x11c]);
			image_size = static_cast<int>(static_cast<off_t>(cylinders * heads * sectors * sector_size));
		}
		else {
			throw io_exception("Invalid NEC image file format");
		}
	}
	else {
		throw io_exception("Invalid NEC image file extension");
	}

	if (sector_size == 0) {
		throw io_exception("Invalid NEC sector size 0");
	}

	// Image size consistency check
	if (image_offset + image_size > size) {
		throw io_exception("NEC image offset/size consistency check failed");
	}

	if (CalculateShiftCount(sector_size) == 0) {
		throw io_exception("Invalid NEC sector size of " + to_string(sector_size) + " byte(s)");
	}

	return { image_size, sector_size };
}

vector<uint8_t> SCSIHD_NEC::InquiryInternal() const
{
	return HandleInquiry(device_type::direct_access, scsi_level::scsi_1_ccs, false);
}

void SCSIHD_NEC::AddFormatPage(map<int, vector<byte>>& pages, bool changeable) const
{
	vector<byte> buf(24);

	// Page can be saved
	buf[0] = (byte)0x80;

	// Make the number of bytes in the physical sector appear mutable (although it cannot actually be)
	if (changeable) {
		SetInt16(buf, 0x0c, -1);

		pages[3] = buf;

		return;
	}

	if (IsReady()) {
		// Set the number of tracks in one zone (PC-9801-55 seems to see this value)
		SetInt16(buf, 0x02, heads);

		// Set the number of sectors per track
		SetInt16(buf, 0x0a, sectors);

		// Set the number of bytes in the physical sector
		SetInt16(buf, 0x0c, GetSectorSizeInBytes());
	}

	// Set removable attributes (remains of the old days)
	if (IsRemovable()) {
		buf[20] = (byte)0x20;
	}

	pages[3] = buf;
}

void SCSIHD_NEC::AddDrivePage(map<int, vector<byte>>& pages, bool changeable) const
{
	vector<byte> buf(20);

	// No changeable area
	if (!changeable && IsReady()) {
		// Set the number of cylinders
		SetInt32(buf, 0x01, cylinders);

		// Set the number of heads
		buf[0x5] = (byte)heads;
	}

	pages[4] = buf;
}

int SCSIHD_NEC::GetInt16LittleEndian(const uint8_t *buf)
{
	return (static_cast<int>(buf[1]) << 8) | buf[0];
}

int SCSIHD_NEC::GetInt32LittleEndian(const uint8_t *buf)
{
	return (static_cast<int>(buf[3]) << 24) | (static_cast<int>(buf[2]) << 16) | (static_cast<int>(buf[1]) << 8) | buf[0];
}
