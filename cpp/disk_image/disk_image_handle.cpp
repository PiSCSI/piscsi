//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//  	Base class for interfacing with disk images.
//
//	[ DiskImageHandle ]
//
//---------------------------------------------------------------------------

#include "disk_image/disk_image_handle.h"

DiskImageHandle::DiskImageHandle(const Filepath &path, int size, uint32_t blocks, off_t imgoff)
{

	serial = 0;
	sec_path = path;
	sec_size = size;
	sec_blocks = blocks;
	imgoffset = imgoff;
}
DiskImageHandle::~DiskImageHandle()
{
}

off_t DiskImageHandle::GetSectorOffset(int block)
{

	int sector_num = block & 0xff;
	return (off_t)sector_num << sec_size;
}

off_t DiskImageHandle::GetTrackOffset(int block)
{

	// Assuming that all tracks hold 256 sectors
	int track_num = block >> 8;

	// Calculate offset (previous tracks are considered to hold 256 sectors)
	off_t offset = ((off_t)track_num << 8);
	if (cd_raw)
	{
		ASSERT(sec_size == 11);
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
