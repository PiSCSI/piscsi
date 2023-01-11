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

#include "posix_file_handle.h"
#include "shared/log.h"
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

//===========================================================================
//
//	Direct file access that will map the file into virtual memory space
//
//===========================================================================
PosixFileHandle::PosixFileHandle(const string &path, int size, uint32_t blocks, off_t imgoff) : DiskImageHandle(path, size, blocks, imgoff)
{
	assert(blocks > 0);
	assert(imgoff >= 0);

	fd = open(path.c_str(), O_RDWR);
	if (fd < 0)
	{
		LOGWARN("Unable to open file %s. Errno:%d", path.c_str(), errno)
		return;
	}
	struct stat sb;
	if (fstat(fd, &sb) < 0)
	{
		LOGWARN("Unable to run fstat. Errno:%d", errno);
		return;
	}

	LOGWARN("%s opened file of size: %d", __PRETTY_FUNCTION__, (unsigned int)sb.st_size);

	initialized = true;
}

PosixFileHandle::~PosixFileHandle()
{
	close(fd);

	// Force the OS to save any cached data to the disk
	sync();
}

bool PosixFileHandle::ReadSector(vector<uint8_t>& buf, int block)
{
	if (!initialized)
	{
		return false;
	}

	assert(sec_size != 0);
	assert(block < sec_blocks);

	size_t sector_size_bytes = (size_t)1 << sec_size;

	// Calculate offset into the image file
	off_t offset = GetTrackOffset(block);
	offset += GetSectorOffset(block);

	lseek(fd, offset, SEEK_SET);
	size_t result = read(fd, buf.data(), sector_size_bytes);
	if (result != sector_size_bytes)
	{
		LOGWARN("%s only read %d bytes but wanted %d", __PRETTY_FUNCTION__, (unsigned int)result, (unsigned int)sector_size_bytes);
	}

	return true;
}

bool PosixFileHandle::WriteSector(const vector<uint8_t>& buf, int block)
{
	if (!initialized)
	{
		return false;
	}

	assert(block < sec_blocks);
	assert(memory_block);

	assert((block * sec_size) <= (sb.st_size + sec_size));

	size_t sector_size_bytes = (size_t)1 << sec_size;

	off_t offset = GetTrackOffset(block);
	offset += GetSectorOffset(block);

	lseek(fd, offset, SEEK_SET);
	size_t result = write(fd, buf.data(), sector_size_bytes);
	if (result != sector_size_bytes)
	{
		LOGWARN("%s only wrote %d bytes but wanted %d ", __PRETTY_FUNCTION__, (unsigned int)result, (unsigned int)sector_size_bytes)
	}

	return true;
}
