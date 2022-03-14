//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	XM6i
//	Copyright (C) 2010-2015 isaki@NetBSD.org
//
//  	Imported sava's Anex86/T98Next image and MO format support patch.
//  	Comments translated to english by akuker.
//
//	[ DiskTrack and DiskCache ]
//
//---------------------------------------------------------------------------

#pragma once

#include "disk_image/disk_image_handle.h"

enum DiskImageHandleType {
	eRamCache,
	eMmapFile,
    ePosixFile,
};

class DiskImageHandleFactory
{
public:
	static void SetFileAccessMethod(DiskImageHandleType method) { current_access_type = method; };
	static DiskImageHandleType GetFileAccessMethod() { return current_access_type; };

    static DiskImageHandle *CreateFileAccess(const Filepath& path, int size, uint32_t blocks, off_t imgoff = 0);

private:
	static DiskImageHandleType current_access_type;
};