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

//===========================================================================
//
//	Disk Track
//
//===========================================================================

DiskTrack::DiskTrack()
{
	// Initialization of internal information
	dt.track = 0;
	dt.size = 0;
	dt.sectors = 0;
	dt.raw = FALSE;
	dt.init = FALSE;
	dt.changed = FALSE;
	dt.length = 0;
	dt.buffer = NULL;
	dt.maplen = 0;
	dt.changemap = NULL;
	dt.imgoffset = 0;
}

DiskTrack::~DiskTrack()
{
	// Release memory, but do not save automatically
	if (dt.buffer) {
		free(dt.buffer);
		dt.buffer = NULL;
	}
	if (dt.changemap) {
		free(dt.changemap);
		dt.changemap = NULL;
	}
}

void DiskTrack::Init(int track, int size, int sectors, BOOL raw, off_t imgoff)
{
	ASSERT(track >= 0);
	ASSERT((sectors > 0) && (sectors <= 0x100));
	ASSERT(imgoff >= 0);

	// Set Parameters
	dt.track = track;
	dt.size = size;
	dt.sectors = sectors;
	dt.raw = raw;

	// Not initialized (needs to be loaded)
	dt.init = FALSE;

	// Not Changed
	dt.changed = FALSE;

	// Offset to actual data
	dt.imgoffset = imgoff;
}

bool DiskTrack::Load(const Filepath& path)
{
	// Not needed if already loaded
	if (dt.init) {
		ASSERT(dt.buffer);
		ASSERT(dt.changemap);
		return true;
	}

	// Calculate offset (previous tracks are considered to hold 256 sectors)
	off_t offset = ((off_t)dt.track << 8);
	if (dt.raw) {
		ASSERT(dt.size == 11);
		offset *= 0x930;
		offset += 0x10;
	} else {
		offset <<= dt.size;
	}

	// Add offset to real image
	offset += dt.imgoffset;

	// Calculate length (data size of this track)
	int length = dt.sectors << dt.size;

	// Allocate buffer memory
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));

	if (dt.buffer == NULL) {
                if (posix_memalign((void **)&dt.buffer, 512, ((length + 511) / 512) * 512)) {
                        LOGWARN("%s posix_memalign failed", __PRETTY_FUNCTION__);
                }
		dt.length = length;
	}

	if (!dt.buffer) {
		return false;
	}

	// Reallocate if the buffer length is different
	if (dt.length != (DWORD)length) {
		free(dt.buffer);
		if (posix_memalign((void **)&dt.buffer, 512, ((length + 511) / 512) * 512)) {
                  LOGWARN("%s posix_memalign failed", __PRETTY_FUNCTION__);  
                }
		dt.length = length;
	}

	// Reserve change map memory
	if (dt.changemap == NULL) {
		dt.changemap = (BOOL *)malloc(dt.sectors * sizeof(BOOL));
		dt.maplen = dt.sectors;
	}

	if (!dt.changemap) {
		return false;
	}

	// Reallocate if the buffer length is different
	if (dt.maplen != (DWORD)dt.sectors) {
		free(dt.changemap);
		dt.changemap = (BOOL *)malloc(dt.sectors * sizeof(BOOL));
		dt.maplen = dt.sectors;
	}

	// Clear changemap
	memset(dt.changemap, 0x00, dt.sectors * sizeof(BOOL));

	// Read from File
	Fileio fio;
	if (!fio.OpenDIO(path, Fileio::ReadOnly)) {
		return false;
	}
	if (dt.raw) {
		// Split Reading
		for (int i = 0; i < dt.sectors; i++) {
			// Seek
			if (!fio.Seek(offset)) {
				fio.Close();
				return false;
			}

			// Read
			if (!fio.Read(&dt.buffer[i << dt.size], 1 << dt.size)) {
				fio.Close();
				return false;
			}

			// Next offset
			offset += 0x930;
		}
	} else {
		// Continuous reading
		if (!fio.Seek(offset)) {
			fio.Close();
			return false;
		}
		if (!fio.Read(dt.buffer, length)) {
			fio.Close();
			return false;
		}
	}
	fio.Close();

	// Set a flag and end normally
	dt.init = TRUE;
	dt.changed = FALSE;
	return true;
}

bool DiskTrack::Save(const Filepath& path)
{
	// Not needed if not initialized
	if (!dt.init) {
		return true;
	}

	// Not needed unless changed
	if (!dt.changed) {
		return true;
	}

	// Need to write
	ASSERT(dt.buffer);
	ASSERT(dt.changemap);
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));

	// Writing in RAW mode is not allowed
	ASSERT(!dt.raw);

	// Calculate offset (previous tracks are considered to hold 256 sectors)
	off_t offset = ((off_t)dt.track << 8);
	offset <<= dt.size;

	// Add offset to real image
	offset += dt.imgoffset;

	// Calculate length per sector
	int length = 1 << dt.size;

	// Open file
	Fileio fio;
	if (!fio.Open(path, Fileio::ReadWrite)) {
		return false;
	}

	// Partial write loop
	int total;
	for (int i = 0; i < dt.sectors;) {
		// If changed
		if (dt.changemap[i]) {
			// Initialize write size
			total = 0;

			// Seek
			if (!fio.Seek(offset + ((off_t)i << dt.size))) {
				fio.Close();
				return false;
			}

			// Consectutive sector length
			int j;
			for (j = i; j < dt.sectors; j++) {
				// end when interrupted
				if (!dt.changemap[j]) {
					break;
				}

				// Add one sector
				total += length;
			}

			// Write
			if (!fio.Write(&dt.buffer[i << dt.size], total)) {
				fio.Close();
				return false;
			}

			// To unmodified sector
			i = j;
		} else {
			// Next Sector
			i++;
		}
	}

	// Close
	fio.Close();

	// Drop the change flag and exit
	memset(dt.changemap, 0x00, dt.sectors * sizeof(BOOL));
	dt.changed = FALSE;
	return true;
}

bool DiskTrack::ReadSector(BYTE *buf, int sec) const
{
	ASSERT(buf);
	ASSERT((sec >= 0) & (sec < 0x100));

	LOGTRACE("%s reading sector: %d", __PRETTY_FUNCTION__,sec);
	// Error if not initialized
	if (!dt.init) {
		return false;
	}

	// // Error if the number of sectors exceeds the valid number
	if (sec >= dt.sectors) {
		return false;
	}

	// Copy
	ASSERT(dt.buffer);
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
	memcpy(buf, &dt.buffer[(off_t)sec << dt.size], (off_t)1 << dt.size);

	// Success
	return true;
}

bool DiskTrack::WriteSector(const BYTE *buf, int sec)
{
	ASSERT(buf);
	ASSERT((sec >= 0) & (sec < 0x100));
	ASSERT(!dt.raw);

	// Error if not initialized
	if (!dt.init) {
		return false;
	}

	// // Error if the number of sectors exceeds the valid number
	if (sec >= dt.sectors) {
		return false;
	}

	// Calculate offset and length
	int offset = sec << dt.size;
	int length = 1 << dt.size;

	// Compare
	ASSERT(dt.buffer);
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
	if (memcmp(buf, &dt.buffer[offset], length) == 0) {
		// Exit normally since it's attempting to write the same thing
		return true;
	}

	// Copy, change
	memcpy(&dt.buffer[offset], buf, length);
	dt.changemap[sec] = TRUE;
	dt.changed = TRUE;

	// Success
	return true;
}

//===========================================================================
//
//	Disk Cache
//
//===========================================================================

DiskCache::DiskCache(const Filepath& path, int size, uint32_t blocks, off_t imgoff)
{
	ASSERT(blocks > 0);
	ASSERT(imgoff >= 0);

	// Cache work
	for (int i = 0; i < CacheMax; i++) {
		cache[i].disktrk = NULL;
		cache[i].serial = 0;
	}

	// Other
	serial = 0;
	sec_path = path;
	sec_size = size;
	sec_blocks = blocks;
	cd_raw = FALSE;
	imgoffset = imgoff;
}

DiskCache::~DiskCache()
{
	// Clear the track
	Clear();
}

void DiskCache::SetRawMode(BOOL raw)
{
	// Configuration
	cd_raw = raw;
}

bool DiskCache::Save()
{
	// Save track
	for (int i = 0; i < CacheMax; i++) {
		// Is it a valid track?
		if (cache[i].disktrk) {
			// Save
			if (!cache[i].disktrk->Save(sec_path)) {
				return false;
			}
		}
	}

	return true;
}

//---------------------------------------------------------------------------
//
//	Get disk cache information
//
//---------------------------------------------------------------------------
bool DiskCache::GetCache(int index, int& track, DWORD& aserial) const
{
	ASSERT((index >= 0) && (index < CacheMax));

	// false if unused
	if (!cache[index].disktrk) {
		return false;
	}

	// Set track and serial
	track = cache[index].disktrk->GetTrack();
	aserial = cache[index].serial;

	return true;
}

void DiskCache::Clear()
{
	// Free the cache
	for (int i = 0; i < CacheMax; i++) {
		if (cache[i].disktrk) {
			delete cache[i].disktrk;
			cache[i].disktrk = NULL;
		}
	}
}

bool DiskCache::ReadSector(BYTE *buf, int block)
{
	ASSERT(sec_size != 0);

	// Update first
	UpdateSerialNumber();

	// Calculate track (fixed to 256 sectors/track)
	int track = block >> 8;

	// Get the track data
	DiskTrack *disktrk = Assign(track);
	if (!disktrk) {
		return false;
	}

	// Read the track data to the cache
	return disktrk->ReadSector(buf, block & 0xff);
}

bool DiskCache::WriteSector(const BYTE *buf, int block)
{
	ASSERT(sec_size != 0);

	// Update first
	UpdateSerialNumber();

	// Calculate track (fixed to 256 sectors/track)
	int track = block >> 8;

	// Get that track data
	DiskTrack *disktrk = Assign(track);
	if (!disktrk) {
		return false;
	}

	// Write the data to the cache
	return disktrk->WriteSector(buf, block & 0xff);
}

//---------------------------------------------------------------------------
//
//	Track Assignment
//
//---------------------------------------------------------------------------
DiskTrack* DiskCache::Assign(int track)
{
	ASSERT(sec_size != 0);
	ASSERT(track >= 0);

	// First, check if it is already assigned
	for (int i = 0; i < CacheMax; i++) {
		if (cache[i].disktrk) {
			if (cache[i].disktrk->GetTrack() == track) {
				// Track match
				cache[i].serial = serial;
				return cache[i].disktrk;
			}
		}
	}

	// Next, check for empty
	for (int i = 0; i < CacheMax; i++) {
		if (!cache[i].disktrk) {
			// Try loading
			if (Load(i, track)) {
				// Success loading
				cache[i].serial = serial;
				return cache[i].disktrk;
			}

			// Load failed
			return NULL;
		}
	}

	// Finally, find the youngest serial number and delete it

	// Set index 0 as candidate c
	DWORD s = cache[0].serial;
	int c = 0;

	// Compare candidate with serial and update to smaller one
	for (int i = 0; i < CacheMax; i++) {
		ASSERT(cache[i].disktrk);

		// Compare and update the existing serial
		if (cache[i].serial < s) {
			s = cache[i].serial;
			c = i;
		}
	}

	// Save this track
	if (!cache[c].disktrk->Save(sec_path)) {
		return NULL;
	}

	// Delete this track
	DiskTrack *disktrk = cache[c].disktrk;
	cache[c].disktrk = NULL;

	if (Load(c, track, disktrk)) {
		// Successful loading
		cache[c].serial = serial;
		return cache[c].disktrk;
	}

	// Load failed
	return NULL;
}

//---------------------------------------------------------------------------
//
//	Load cache
//
//---------------------------------------------------------------------------
bool DiskCache::Load(int index, int track, DiskTrack *disktrk)
{
	ASSERT((index >= 0) && (index < CacheMax));
	ASSERT(track >= 0);
	ASSERT(!cache[index].disktrk);

	// Get the number of sectors on this track
	int sectors = sec_blocks - (track << 8);
	ASSERT(sectors > 0);
	if (sectors > 0x100) {
		sectors = 0x100;
	}

	// Create a disk track
	if (disktrk == NULL) {
		disktrk = new DiskTrack();
	}

	// Initialize disk track
	disktrk->Init(track, sec_size, sectors, cd_raw, imgoffset);

	// Try loading
	if (!disktrk->Load(sec_path)) {
		// Failure
		delete disktrk;
		return false;
	}

	// Allocation successful, work set
	cache[index].disktrk = disktrk;

	return true;
}

void DiskCache::UpdateSerialNumber()
{
	// Update and do nothing except 0
	serial++;
	if (serial != 0) {
		return;
	}

	// Clear serial of all caches (loop in 32bit)
	for (int i = 0; i < CacheMax; i++) {
		cache[i].serial = 0;
	}
}

