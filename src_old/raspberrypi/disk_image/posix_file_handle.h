//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//	[ PosixFileHandle ]
//
//---------------------------------------------------------------------------

#pragma once

#include "filepath.h"
#include "disk_image/disk_image_handle.h"

class PosixFileHandle : public DiskImageHandle
{

public:
	PosixFileHandle(const Filepath &path, int size, uint32_t blocks, off_t imgoff = 0);
	~PosixFileHandle();

	void SetRawMode(BOOL raw); // CD-ROM raw mode setting

	// Access
	bool Save() { return true; };												// Save and release all
	bool ReadSector(BYTE *buf, int block);										// Sector Read
	bool WriteSector(const BYTE *buf, int block);								// Sector Write
	bool GetCache(int index, int &track, DWORD &serial) const { return true; }; // Get cache information

private:
	int fd;
	bool initialized = false;
};
