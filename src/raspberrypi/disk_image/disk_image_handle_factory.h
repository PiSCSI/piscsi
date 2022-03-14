//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//  	Factory class for creating DiskImageHandles
//
//	[ DiskImageHandleFactory ]
//
//---------------------------------------------------------------------------

#pragma once

#include "disk_image/disk_image_handle.h"

enum DiskImageHandleType
{
	eRamCache,
	eMmapFile,
	ePosixFile,
};

class DiskImageHandleFactory
{
public:
	static void SetFileAccessMethod(DiskImageHandleType method) { current_access_type = method; };
	static DiskImageHandleType GetFileAccessMethod() { return current_access_type; };

	static DiskImageHandle *CreateDiskImageHandle(const Filepath &path, int size, uint32_t blocks, off_t imgoff = 0);

private:
	static DiskImageHandleType current_access_type;
};