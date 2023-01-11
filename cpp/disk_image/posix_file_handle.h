//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2022-2023 akuker
//
//	[ PosixFileHandle ]
//
//---------------------------------------------------------------------------

#pragma once

#include "disk_image/disk_image_handle.h"

class PosixFileHandle : public DiskImageHandle
{

public:
	PosixFileHandle(const string &path, int size, uint32_t blocks, off_t imgoff = 0);
	~PosixFileHandle() override;

	// Access
	bool Save() override  { return true; };												// Save and release all
	bool ReadSector(vector<uint8_t>& buf, int block) override ;										// Sector Read
	bool WriteSector(const vector<uint8_t>& buf, int block) override ;								// Sector Write
	bool GetCache(int index, int &track, uint32_t &serial) const override { (void)index; (void)track; (void)serial; return true; }; // Get cache information

private:
	int fd;
	bool initialized = false;
};
