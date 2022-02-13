//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2021 akuker
//
//	[ PosixFileAccess ]
//
//---------------------------------------------------------------------------

#include "posix_file_access.h"
#include "log.h"
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

//===========================================================================
//
//	Direct file access that will map the file into virtual memory space
//
//===========================================================================
PosixFileAccess::PosixFileAccess(const Filepath& path, int size, uint32_t blocks, off_t imgoff) : FileAccess(path, size, blocks, imgoff)
{
	ASSERT(blocks > 0);
	ASSERT(imgoff >= 0);


   fd = open(path.GetPath(), O_RDWR);
   if(fd < 0){
	   LOGWARN("Unable to open file %s. Errno:%d", path.GetPath(), errno)
	   return;
   }
	struct stat sb;
   if(fstat(fd, &sb) < 0){
	   LOGWARN("Unable to run fstat. Errno:%d", errno);
	   return;
   }

   LOGWARN("%s opened file of size: %llu", __PRETTY_FUNCTION__, (uint64_t)sb.st_size);

	initialized = true;
}

PosixFileAccess::~PosixFileAccess()
{
	close(fd);

	// Force the OS to save any cached data to the disk
	sync();
}

bool PosixFileAccess::ReadSector(BYTE *buf, int block)
{
	if(!initialized){
		return false;
	}

	ASSERT(sec_size != 0);
	ASSERT(buf);
	ASSERT(block < sec_blocks);
	ASSERT(memory_block);

	size_t sector_size_bytes = (size_t)1 << sec_size;

	// Calculate offset into the image file
	off_t offset = GetTrackOffset(block);
	offset += GetSectorOffset(block);

    lseek(fd, offset, SEEK_SET);
    size_t result = read(fd, buf,sector_size_bytes);
    if(result != sector_size_bytes){
        LOGWARN("%s only read %d bytes but wanted %d ", __PRETTY_FUNCTION__, result, sector_size_bytes);
    }

	return true;
}

bool PosixFileAccess::WriteSector(const BYTE *buf, int block)
{
	if(!initialized){
		return false;
	}

	ASSERT(buf);
	ASSERT(block < sec_blocks);
	ASSERT(memory_block);

	ASSERT((block * sec_size) <= (sb.st_size + sec_size));

	size_t sector_size_bytes = (size_t)1 << sec_size;

    off_t offset = GetTrackOffset(block);
    offset += GetSectorOffset(block);


    lseek(fd, offset, SEEK_SET);
    size_t result = write(fd, buf,sector_size_bytes);
    if(result != sector_size_bytes){
        LOGWARN("%s only wrote %d bytes but wanted %d ", __PRETTY_FUNCTION__, result, sector_size_bytes);
    }

	return true;
}


