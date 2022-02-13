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

#include "file_access/file_access.h"

enum FileAccessType {
	eRamCache,
	eMmapFile,
    ePosixFile,
};

class FileAccessFactory
{
public:
	static void SetFileAccessMethod(FileAccessType method) { current_access_type = method; };
	static FileAccessType GetFileAccessMethod() { return current_access_type; };

    static FileAccess *CreateFileAccess(const Filepath& path, int size, uint32_t blocks, off_t imgoff = 0);

private:
	static FileAccessType current_access_type;
};