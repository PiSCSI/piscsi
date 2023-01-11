//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2022-2023 akuker
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

#include <string>
#include <sys/stat.h>
#include "disk_image/disk_image_handle.h"

using namespace std;

class MmapFileHandle : public DiskImageHandle
{

public:
	MmapFileHandle(const string &path, int size, uint32_t blocks, off_t imgoff = 0);
	~MmapFileHandle() override;

	// Access
	bool Save()  override { return true; };												// Save and release all
	bool ReadSector(vector<uint8_t>& buf, int block) override;										// Sector Read
	bool WriteSector(const vector<uint8_t>& buf, int block) override;								// Sector Write
	bool GetCache(int index, int &track, uint32_t &serial) const override { (void)index; (void)track; (void)serial; return true; }; // Get cache information

private:
	const char *memory_block;
	struct stat sb;
	int fd;

	bool initialized = false;
};
