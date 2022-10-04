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
//---------------------------------------------------------------------------

#include "fileio.h"
#include "rascsi_exceptions.h"
#include "scsi_command_util.h"
#include "scsimo.h"

using namespace scsi_command_util;

SCSIMO::SCSIMO(int lun, const unordered_set<uint32_t>& sector_sizes) : Disk("SCMO", lun)
{
	SetSectorSizes(sector_sizes);

	// 128 MB, 512 bytes per sector, 248826 sectors
	geometries[512 * 248826] = make_pair(512, 248826);
	// 230 MB, 512 bytes per block, 446325 sectors
	geometries[512 * 446325] = make_pair(512, 446325);
	// 540 MB, 512 bytes per sector, 1041500 sectors
	geometries[512 * 1041500] = make_pair(512, 1041500);
	// 640 MB, 20248 bytes per sector, 310352 sectors
	geometries[2048 * 310352] = make_pair(2048, 310352);
}

void SCSIMO::Open(const Filepath& path)
{
	assert(!IsReady());

	// Open as read-only
	Fileio fio;

	if (!fio.Open(path, Fileio::OpenMode::ReadOnly)) {
		throw file_not_found_exception("Can't open MO file");
	}

	// Get file size
	off_t size = fio.GetFileSize();
	fio.Close();

	// For some capacities there are hard-coded, well-defined sector sizes and block counts
	if (!SetGeometryForCapacity(size)) {
		// Sector size (default 512 bytes) and number of blocks
		SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 512);
		SetBlockCount(size >> GetSectorSizeShiftCount());
	}

	SetReadOnly(false);
	SetProtectable(true);
	SetProtected(false);

	super::Open(path);
	SetPath(path);

	SetUpCache(path, 0);

	// Attention if ready
	if (IsReady()) {
		SetAttn(true);
	}
}

vector<byte> SCSIMO::InquiryInternal() const
{
	return HandleInquiry(device_type::OPTICAL_MEMORY, scsi_level::SCSI_2, true);
}

void SCSIMO::SetUpModePages(map<int, vector<byte>>& pages, int page, bool changeable) const
{
	Disk::SetUpModePages(pages, page, changeable);

	// Page code 6
	if (page == 0x06 || page == 0x3f) {
		AddOptionPage(pages, changeable);
	}
}

void SCSIMO::AddFormatPage(map<int, vector<byte>>& pages, bool changeable) const
{
	Disk::AddFormatPage(pages, changeable);

	EnrichFormatPage(pages, changeable, 1 << GetSectorSizeShiftCount());
}

void SCSIMO::AddOptionPage(map<int, vector<byte>>& pages, bool) const
{
	vector<byte> buf(4);
	pages[6] = buf;

	// Do not report update blocks
}

void SCSIMO::ModeSelect(const vector<int>& cdb, const vector<BYTE>& buf, int length) const
{
	scsi_command_util::ModeSelect(cdb, buf, length, 1 << GetSectorSizeShiftCount());
}

//---------------------------------------------------------------------------
//
//	Vendor Unique Format Page 20h (MO)
//
//---------------------------------------------------------------------------
void SCSIMO::AddVendorPage(map<int, vector<byte>>& pages, int page, bool changeable) const
{
	// Page code 20h
	if (page != 0x20 && page != 0x3f) {
		return;
	}

	vector<byte> buf(12);

	// No changeable area
	if (changeable) {
		pages[32] = buf;

		return;
	}

	/*
		mode page code 20h - Vendor Unique Format Page
		format mode XXh type 0
		information: http://h20628.www2.hp.com/km-ext/kmcsdirect/emr_na-lpg28560-1.pdf

		offset  description
		  02h   format mode
		  03h   type of format (0)
		04~07h  size of user band (total sectors?)
		08~09h  size of spare band (spare sectors?)
		0A~0Bh  number of bands

		actual value of each 3.5inches optical medium (grabbed by Fujitsu M2513EL)

		                     128M     230M    540M    640M
		---------------------------------------------------
		size of user band   3CBFAh   6CF75h  FE45Ch  4BC50h
		size of spare band   0400h    0401h   08CAh   08C4h
		number of bands      0001h    000Ah   0012h   000Bh

		further information: http://r2089.blog36.fc2.com/blog-entry-177.html
	*/

	if (IsReady()) {
		unsigned spare = 0;
		unsigned bands = 0;
		uint64_t blocks = GetBlockCount();

		if (GetSectorSizeInBytes() == 512) {
			switch (blocks) {
				// 128MB
				case 248826:
					spare = 1024;
					bands = 1;
					break;

				// 230MB
				case 446325:
					spare = 1025;
					bands = 10;
					break;

				// 540MB
				case 1041500:
					spare = 2250;
					bands = 18;
					break;

				default:
					break;
			}
		}

		if (GetSectorSizeInBytes() == 2048) {
			switch (blocks) {
				// 640MB
				case 310352:
					spare = 2244;
					bands = 11;
					break;

					// 1.3GB (lpproj: not tested with real device)
				case 605846:
					spare = 4437;
					bands = 18;
					break;

				default:
					break;
			}
		}

		buf[2] = (byte)0; // format mode
		buf[3] = (byte)0; // type of format
		SetInt32(buf, 4, (uint32_t)blocks);
		SetInt16(buf, 8, spare);
		SetInt16(buf, 10, bands);
	}

	pages[32] = buf;

	return;
}

bool SCSIMO::SetGeometryForCapacity(uint64_t capacity) {
	if (const auto& geometry = geometries.find(capacity); geometry != geometries.end()) {
		SetSectorSizeInBytes(geometry->second.first);
		SetBlockCount(geometry->second.second);

		return true;
	}

	return false;
}
