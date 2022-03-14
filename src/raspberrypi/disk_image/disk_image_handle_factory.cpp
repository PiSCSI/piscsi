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

#include "disk_image/disk_image_handle_factory.h"
#include "log.h"
#include "disk_image/disk_track_cache.h"
#include "disk_image/mmap_file_handle.h"
#include "disk_image/posix_file_handle.h"

DiskImageHandleType DiskImageHandleFactory::current_access_type = DiskImageHandleType::ePosixFile;

DiskImageHandle *DiskImageHandleFactory::CreateFileAccess(const Filepath& path, int size, uint32_t blocks, off_t imgoff){

    DiskImageHandle *result = NULL;

    if (current_access_type == DiskImageHandleType::eMmapFile){
        LOGINFO("%s Creating MmapFileAccess %s", __PRETTY_FUNCTION__, path.GetPath())
        result = new MmapFileHandle(path, size, blocks, imgoff);
    }
    else if(current_access_type == DiskImageHandleType::eRamCache) {
        LOGINFO("%s Creating DiskCache %s", __PRETTY_FUNCTION__, path.GetPath())
        result = new DiskCache(path, size, blocks, imgoff);
    }
    else if(current_access_type == DiskImageHandleType::ePosixFile) {
        LOGINFO("%s Creating PosixFileHandle %s", __PRETTY_FUNCTION__, path.GetPath())
        result = new PosixFileHandle(path, size, blocks, imgoff);
    }

    if (result == NULL){
        LOGWARN("%s Unable to create the File Access", __PRETTY_FUNCTION__);
    }
    return result;

}

