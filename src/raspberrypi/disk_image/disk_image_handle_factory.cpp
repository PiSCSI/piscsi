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

#include "disk_image/disk_image_handle_factory.h"
#include "log.h"
#include "disk_image/disk_track_cache.h"
#include "disk_image/mmap_file_handle.h"
#include "disk_image/posix_file_handle.h"

DiskImageHandleType DiskImageHandleFactory::current_access_type = DiskImageHandleType::ePosixFile;

DiskImageHandle *DiskImageHandleFactory::CreateDiskImageHandle(const Filepath &path, int size, uint32_t blocks, off_t imgoff)
{

    DiskImageHandle *result = NULL;

    if (current_access_type == DiskImageHandleType::eMmapFile)
    {
        LOGINFO("%s Creating MmapFileAccess %s", __PRETTY_FUNCTION__, path.GetPath())
        result = new MmapFileHandle(path, size, blocks, imgoff);
    }
    else if (current_access_type == DiskImageHandleType::eRamCache)
    {
        LOGINFO("%s Creating DiskCache %s", __PRETTY_FUNCTION__, path.GetPath())
        result = new DiskCache(path, size, blocks, imgoff);
    }
    else if (current_access_type == DiskImageHandleType::ePosixFile)
    {
        LOGINFO("%s Creating PosixFileHandle %s", __PRETTY_FUNCTION__, path.GetPath())
        result = new PosixFileHandle(path, size, blocks, imgoff);
    }

    if (result == NULL)
    {
        LOGWARN("%s Unable to create the File Access", __PRETTY_FUNCTION__);
    }
    return result;
}
