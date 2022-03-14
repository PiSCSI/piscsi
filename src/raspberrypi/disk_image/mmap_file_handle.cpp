//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
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
//	[ MmapFilehandle ]
//
//---------------------------------------------------------------------------

#include "mmap_file_handle.h"
#include "log.h"
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

//===========================================================================
//
//	Direct file access that will map the file into virtual memory space
//
//===========================================================================
MmapFileHandle::MmapFileHandle(const Filepath &path, int size, uint32_t blocks, off_t imgoff) : DiskImageHandle(path, size, blocks, imgoff)
{
	ASSERT(blocks > 0);
	ASSERT(imgoff >= 0);

	fd = open(path.GetPath(), O_RDWR);
	if (fd < 0)
	{
		LOGWARN("Unable to open file %s. Errno:%d", path.GetPath(), errno)
	}
	LOGWARN("%s opened %s", __PRETTY_FUNCTION__, path.GetPath());
	struct stat sb;
	if (fstat(fd, &sb) < 0)
	{
		LOGWARN("Unable to run fstat. Errno:%d", errno);
	}
	printf("Size: %llu\n", (uint64_t)sb.st_size);

	LOGWARN("%s mmap-ed file of size: %llu", __PRETTY_FUNCTION__, (uint64_t)sb.st_size);

	// int x =    EACCES;

	memory_block = (const char *)mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	int errno_val = errno;
	if (memory_block == MAP_FAILED)
	{
		LOGWARN("Unabled to memory map file %s", path.GetPath());
		LOGWARN("   Errno:%d", errno_val);
		return;
	}
	initialized = true;
}

MmapFileHandle::~MmapFileHandle()
{
	munmap((void *)memory_block, sb.st_size);
	close(fd);

	// Force the OS to save any cached data to the disk
	sync();
}

bool MmapFileHandle::ReadSector(BYTE *buf, int block)
{
	ASSERT(sec_size != 0);
	ASSERT(buf);
	ASSERT(block < sec_blocks);
	ASSERT(memory_block);

	int sector_size_bytes = (off_t)1 << sec_size;

	// Calculate offset into the image file
	off_t offset = GetTrackOffset(block);
	offset += GetSectorOffset(block);

	memcpy(buf, &memory_block[offset], sector_size_bytes);

	return true;
}

bool MmapFileHandle::WriteSector(const BYTE *buf, int block)
{

	ASSERT(buf);
	ASSERT(block < sec_blocks);
	ASSERT(memory_block);

	ASSERT((block * sec_size) <= (sb.st_size + sec_size));

	memcpy((void *)&memory_block[(block * sec_size)], buf, sec_size);

	return true;
}
