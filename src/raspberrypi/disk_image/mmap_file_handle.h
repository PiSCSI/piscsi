//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//  This method of file access will use the mmap() capabilities to 'map' the
//  file into memory.
//
//  This doesn't actually copy the file into RAM, but instead maps the file
//  as virtual memory. Behind the scenes, the file is still being accessed.
//
//  The operating system will do some caching of the data to prevent direct
//  drive access for each read/write. So, instead of implementing our own
//  caching mechanism (like in disk_track_cache.h), we just rely on the
//  operating system.
//
//	[ MmapFileHandle ]
//
//---------------------------------------------------------------------------

#pragma once

#include "filepath.h"
#include "disk_image/disk_image_handle.h"

class MmapFileHandle : public DiskImageHandle
{

public:
	MmapFileHandle(const Filepath &path, int size, uint32_t blocks, off_t imgoff = 0);
	~MmapFileHandle();

	void SetRawMode(BOOL raw); // CD-ROM raw mode setting

	// Access
	bool Save() { return true; };												// Save and release all
	bool ReadSector(BYTE *buf, int block);										// Sector Read
	bool WriteSector(const BYTE *buf, int block);								// Sector Write
	bool GetCache(int index, int &track, DWORD &serial) const { return true; }; // Get cache information

private:
	const char *memory_block;
	struct stat sb;
	int fd;

	bool initialized = false;
};
