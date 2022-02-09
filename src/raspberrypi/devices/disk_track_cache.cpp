//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	XM6i
//	Copyright (C) 2010-2015 isaki@NetBSD.org
//	Copyright (C) 2010 Y.Sugahara
//
//	Imported sava's Anex86/T98Next image and MO format support patch.
//	Imported NetBSD support and some optimisation patch by Rin Okuyama.
//  	Comments translated to english by akuker.
//
//	[ DiskTrack and DiskCache ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "log.h"
#include "fileio.h"
#include "disk_track_cache.h"
#include <sys/mman.h>
#include <errno.h>

//===========================================================================
//
//	Disk Cache
//
//===========================================================================

DiskCache::DiskCache(const Filepath& path, int size, uint32_t blocks, off_t imgoff)
{
	ASSERT(blocks > 0);
	ASSERT(imgoff >= 0);

	initialized = false;

	// Other
	serial = 0;
	sec_path = path;
	sec_size = size;
	sec_blocks = blocks;
	cd_raw = FALSE;
	imgoffset = imgoff;

   fd = open(path.GetPath(), O_RDWR);
   if(fd < 0){
	   LOGWARN("Unable to open file %s. Errno:%d", path.GetPath(), errno)
   }
	struct stat sb;
   if(fstat(fd, &sb) < 0){
	   LOGWARN("Unable to run fstat. Errno:%d", errno);
   }
   printf("Size: %llu\n", (uint64_t)sb.st_size);

	// int x =    EACCES;

   	memory_block = (const char*)mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	int errno_val = errno;
   if (memory_block == MAP_FAILED) {
	   LOGWARN("Unabled to memory map file %s", path.GetPath());
	   LOGWARN("   Errno:%d", errno_val);
	   errno_val = EACCES;
   }


	initialized = true;

}

DiskCache::~DiskCache()
{
	munmap((void*)memory_block, sb.st_size);
	close(fd);
	// Clear the track
	// Clear();
}

void DiskCache::SetRawMode(BOOL raw)
{
	// Configuration
	cd_raw = raw;
}


bool DiskCache::ReadSector(BYTE *buf, int block)
{
	ASSERT(sec_size != 0);
	ASSERT(buf);
	ASSERT(block < sec_blocks);
	ASSERT(memory_block);

	if(!initialized){
		return false;
	}

	int sector_size_bytes = (off_t)1 << sec_size;

	// Calculate offset into the image file
	off_t offset = GetTrackOffset(block);
	offset += GetSectorOffset(block);
	LOGWARN("%s track:%d sector:%d offset:%llu block:%d size:%d", __PRETTY_FUNCTION__, (block >>8), (block & 0xff), offset, block, sector_size_bytes);

	// LOGINFO("Reading data track:%d sec_size:%d  block:%d offset: %lld imgoffset: %lld", track, sec_size, block, offset, imgoffset);

	memcpy(buf, &memory_block[offset], sector_size_bytes);

	return true;

}

bool DiskCache::WriteSector(const BYTE *buf, int block)
{
	ASSERT(buf);
	ASSERT(block < sec_blocks);
	ASSERT(memory_block);

	if(!initialized){
		return false;
	}

	int sector_size_bytes = (off_t)1 << sec_size;

	// Calculate offset into the image file
	off_t offset = GetTrackOffset(block);
	offset += GetSectorOffset(block);
	LOGWARN("%s track:%d sector:%d offset:%llu block:%d size:%d", __PRETTY_FUNCTION__, (block >>8), (block & 0xff), offset, block, sector_size_bytes);

	memcpy((void*)&memory_block[offset], buf, sector_size_bytes);

	return true;
}


off_t DiskCache::GetSectorOffset(int block){

	int sector_num = block & 0xff;

	// // // Error if the number of sectors exceeds the valid number
	// if (sector_num >= sectors) {
	// 	return false;
	// }

	return (off_t)sector_num << sec_size;
}

off_t DiskCache::GetTrackOffset(int block){

	// Assuming that all tracks hold 256 sectors
	int track_num = block >> 8;

	// Calculate offset (previous tracks are considered to hold 256 sectors)
	off_t offset = ((off_t)track_num << 8);
	if (cd_raw) {
		ASSERT(sec_size == 11);
		offset *= 0x930;
		offset += 0x10;
	} else {
		offset <<= sec_size;
	}

	// Add offset to real image
	offset += imgoffset;

	return offset;
}

// off_t DiskCache::GetTrackSize(){
// 	// Calculate length (data size of this track)
// 	off_t length = sectors << sec_size;
// 	return length;
// }
