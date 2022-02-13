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

#include "file_access/file_access_factory.h"
#include "log.h"
#include "file_access/disk_track_cache.h"
#include "file_access/mmap_file_access.h"
#include "file_access/posix_file_access.h"

FileAccessType FileAccessFactory::current_access_type = FileAccessType::ePosixFile;

FileAccess *FileAccessFactory::CreateFileAccess(const Filepath& path, int size, uint32_t blocks, off_t imgoff){

    FileAccess *result = NULL;

    if (current_access_type == FileAccessType::eMmapFile){
        LOGINFO("%s Creating MmapFileAccess %s", __PRETTY_FUNCTION__, path.GetPath())
        result = new MmapFileAccess(path, size, blocks, imgoff);
    }
    else if(current_access_type == FileAccessType::eRamCache) {
        LOGINFO("%s Creating DiskCache %s", __PRETTY_FUNCTION__, path.GetPath())
        result = new DiskCache(path, size, blocks, imgoff);
    }
    else if(current_access_type == FileAccessType::ePosixFile) {
        LOGINFO("%s Creating PosixFileAccess %s", __PRETTY_FUNCTION__, path.GetPath())
        result = new PosixFileAccess(path, size, blocks, imgoff);
    }

    if (result == NULL){
        LOGWARN("%s Unable to create the File Access", __PRETTY_FUNCTION__);
    }
    return result;

}

