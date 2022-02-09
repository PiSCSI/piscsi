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

   if(sb.st_size != (sec_size * sec_blocks)){
	   LOGWARN("Opened file with size %llu bytes, but expected %llu [sector: %d blocks: %d]", sb.st_size, (uint64_t)(sec_size * sec_blocks), sec_size, sec_blocks);
   }

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

	int sector_size_bytes = (off_t)1 << sec_size;

	// Calculate offset into the image file
	off_t offset = GetTrackOffset(block);
	offset += GetSectorOffset(block);

	// LOGINFO("Reading data track:%d sec_size:%d  block:%d offset: %lld imgoffset: %lld", track, sec_size, block, offset, imgoffset);

// 	memcpy(buf, &dt.buffer[(off_t)sec << dt.size], (off_t)1 << dt.size);
	memcpy(buf, &memory_block[offset], sector_size_bytes);

	return true;


	// // Calculate track (fixed to 256 sectors/track)
	// int track = block >> 8;

	// // Get the track data
	// DiskTrack *disktrk = Assign(track);
	// if (!disktrk) {
	// 	return false;
	// }

	// Read the track data to the cache
	// return disktrk->ReadSector(buf, block & 0xff);
}

bool DiskCache::WriteSector(const BYTE *buf, int block)
{
	// ASSERT(sec_size != 0);

	// // Update first
	// // UpdateSerialNumber();

	// // Calculate track (fixed to 256 sectors/track)
	// int track = block >> 8;

	// // Get that track data
	// DiskTrack *disktrk = Assign(track);
	// if (!disktrk) {
	// 	return false;
	// }

	// // Write the data to the cache
	// return disktrk->WriteSector(buf, block & 0xff);


	// ASSERT(sec_size != 0);

	// Update first
	// UpdateSerialNumber();


	// // Other
	// serial = 0;
	// sec_path = path;
	// sec_size = size;
	// sec_blocks = blocks;
	// cd_raw = FALSE;
	// imgoffset = imgoff;

// 	ASSERT(dt.buffer);
// 	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
// 	memcpy(buf, &dt.buffer[(off_t)sec << dt.size], (off_t)1 << dt.size);


	ASSERT(buf);
	ASSERT(block < sec_blocks);
	ASSERT(memory_block);

	ASSERT((block * sec_size) <= (sb.st_size + sec_size));

// 	memcpy(buf, &dt.buffer[(off_t)sec << dt.size], (off_t)1 << dt.size);
	memcpy((void*)&memory_block[(block * sec_size)], buf, sec_size);

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
