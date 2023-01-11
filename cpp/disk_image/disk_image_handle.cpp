//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2022-2023 akuker
//
//  	Base class for interfacing with disk images.
//
//	[ DiskImageHandle ]
//
//---------------------------------------------------------------------------

#include <cassert>
#include <string_view>
#include "disk_image/disk_image_handle.h"

DiskImageHandle::DiskImageHandle(string_view path, int size, uint32_t blocks, off_t imgoff)
	: sec_path(path), sec_size(size), sec_blocks(blocks), imgoffset(imgoff)
{
	assert(blocks > 0);
	assert(imgoff >= 0);
	assert(sec_size > 0);
}

off_t DiskImageHandle::GetSectorOffset(int block) const
{

	int sector_num = block & 0xff;
	return (off_t)sector_num << sec_size;
}

off_t DiskImageHandle::GetTrackOffset(int block) const
{

	// Assuming that all tracks hold 256 sectors
	int track_num = block >> 8;

	// Calculate offset (previous tracks are considered to hold 256 sectors)
	off_t offset = ((off_t)track_num << 8);
	if (cd_raw)
	{
		assert(sec_size == 11);
		offset *= 0x930;
		offset += 0x10;
	}
	else
	{
		offset <<= sec_size;
	}

	// Add offset to real image
	offset += imgoffset;

	return offset;
}
