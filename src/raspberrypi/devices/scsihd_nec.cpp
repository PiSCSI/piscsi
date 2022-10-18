//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License.
//	See LICENSE file in the project root folder.
//
//	[ SCSI NEC "Genuine" Hard Disk]
//
//---------------------------------------------------------------------------

#include "scsihd_nec.h"
#include "fileio.h"
#include "rascsi_exceptions.h"
#include "scsi_command_util.h"

using namespace scsi_command_util;

const unordered_set<uint32_t> SCSIHD_NEC::sector_sizes = { 512 };

void SCSIHD_NEC::Open()
{
	assert(!IsReady());

	// Open as read-only
	Filepath path;
	path.SetPath(GetFilename().c_str());
	Fileio fio;
	if (!fio.Open(path, Fileio::OpenMode::ReadOnly)) {
		throw file_not_found_exception("Can't open hard disk file");
	}

	off_t size = GetFileSize();

	// NEC root sector
	array<BYTE, 512> root_sector;
	if (size < (off_t)root_sector.size() || !fio.Read(root_sector.data(), root_sector.size())) {
		fio.Close();
		throw io_exception("Can't read NEC hard disk file root sector");
	}
	fio.Close();

	// Effective size must be a multiple of 512
	size = (size / 512) * 512;

	// Determine parameters by extension
	const auto [image_size, sector_size] = SetParameters(path.GetFileExt(), root_sector, (int)size);

	if (sector_size == 0) {
		throw io_exception("Invalid NEC drive sector size");
	}

	// Image size consistency check
	if (image_offset + image_size > size || image_size % sector_size != 0) {
		throw io_exception("Image size consistency check failed");
	}

	// Calculate sector size
	for (size = 16; size > 0; --size) {
		if ((1 << size) == sector_size)
			break;
	}
	if (size <= 0 || size > 16) {
		throw io_exception("Invalid NEC disk size");
	}
	SetSectorSizeShiftCount((uint32_t)size);

	SetBlockCount(image_size >> GetSectorSizeShiftCount());

	FinalizeSetup(size, image_offset);
}

pair<int, int> SCSIHD_NEC::SetParameters(const string& extension, const array<BYTE, 512>& root_sector, int size)
{
	string ext = extension;
	transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	int image_size = 0;
	int sector_size = 0;

	// PC-9801-55 NEC genuine?
	if (ext == ".hdn") {
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
	else if (ext == ".hdi") {
		image_offset = GetInt32LittleEndian(&root_sector[8]);
		image_size = GetInt32LittleEndian(&root_sector[12]);
		sector_size = GetInt32LittleEndian(&root_sector[16]);
		sectors = GetInt32LittleEndian(&root_sector[20]);
		heads = GetInt32LittleEndian(&root_sector[24]);
		cylinders = GetInt32LittleEndian(&root_sector[28]);
	}
	// T98Next HD image?
	else if (ext == ".nhd") {
		if (!memcmp(root_sector.data(), "T98HDDIMAGE.R0\0", 15)) {
			image_offset = GetInt32LittleEndian(&root_sector[0x110]);
			cylinders = GetInt32LittleEndian(&root_sector[0x114]);
			heads = GetInt16LittleEndian(&root_sector[0x118]);
			sectors = GetInt16LittleEndian(&root_sector[0x11a]);
			sector_size = GetInt16LittleEndian(&root_sector[0x11c]);
			image_size = (int)((off_t)cylinders * heads * sectors * sector_size);
		}
		else {
			throw io_exception("Invalid NEC image file format");
		}
	}
	else {
		assert(false);
	}

	return make_pair(image_size, sector_size);
}

vector<byte> SCSIHD_NEC::InquiryInternal() const
{
	return HandleInquiry(device_type::DIRECT_ACCESS, scsi_level::SCSI_1_CCS, false);
}

void SCSIHD_NEC::AddErrorPage(map<int, vector<byte>>& pages, bool) const
{
	vector<byte> buf(8);

	// The retry count is 0, and the limit time uses the default value inside the device.

	pages[1] = buf;
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

int SCSIHD_NEC::GetInt16LittleEndian(const BYTE *buf)
{
	return ((int)buf[1] << 8) | buf[0];
}

int SCSIHD_NEC::GetInt32LittleEndian(const BYTE *buf)
{
	return ((int)buf[3] << 24) | ((int)buf[2] << 16) | ((int)buf[1] << 8) | buf[0];
}
