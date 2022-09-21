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

DiskTrack::~DiskTrack()
{
	// Release memory, but do not save automatically
	if (dt.buffer) {
		free(dt.buffer);
	}
	if (dt.changemap) {
		free(dt.changemap);
	}
}

void DiskTrack::Init(int track, int size, int sectors, bool raw, off_t imgoff)
{
	assert(track >= 0);
	assert((sectors > 0) && (sectors <= 0x100));
	assert(imgoff >= 0);

	// Set Parameters
	dt.track = track;
	dt.size = size;
	dt.sectors = sectors;
	dt.raw = raw;

	// Not initialized (needs to be loaded)
	dt.init = false;

	// Not Changed
	dt.changed = false;

	// Offset to actual data
	dt.imgoffset = imgoff;
}

bool DiskTrack::Load(const Filepath& path)
{
	// Not needed if already loaded
	if (dt.init) {
		assert(dt.buffer);
		assert(dt.changemap);
		return true;
	}

	// Calculate offset (previous tracks are considered to hold 256 sectors)
	off_t offset = ((off_t)dt.track << 8);
	if (dt.raw) {
		assert(dt.size == 11);
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
	assert((dt.sectors > 0) && (dt.sectors <= 0x100));

	if (dt.buffer == nullptr) {
		if (posix_memalign((void **)&dt.buffer, 512, ((length + 511) / 512) * 512)) {
			LOGWARN("%s posix_memalign failed", __PRETTY_FUNCTION__)
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
			LOGWARN("%s posix_memalign failed", __PRETTY_FUNCTION__)
        }
		dt.length = length;
	}

	// Reserve change map memory
	if (dt.changemap == nullptr) {
		dt.changemap = (bool *)malloc(dt.sectors * sizeof(bool));
		dt.maplen = dt.sectors;
	}

	if (!dt.changemap) {
		return false;
	}

	// Reallocate if the buffer length is different
	if (dt.maplen != (DWORD)dt.sectors) {
		free(dt.changemap);
		dt.changemap = (bool *)malloc(dt.sectors * sizeof(bool));
		dt.maplen = dt.sectors;
	}

	// Clear changemap
	memset(dt.changemap, 0x00, dt.sectors * sizeof(bool));

	// Read from File
	Fileio fio;
	if (!fio.OpenDIO(path, Fileio::OpenMode::ReadOnly)) {
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
	dt.init = true;
	dt.changed = false;
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
	assert(dt.buffer);
	assert(dt.changemap);
	assert((dt.sectors > 0) && (dt.sectors <= 0x100));

	// Writing in RAW mode is not allowed
	assert(!dt.raw);

	// Calculate offset (previous tracks are considered to hold 256 sectors)
	off_t offset = ((off_t)dt.track << 8);
	offset <<= dt.size;

	// Add offset to real image
	offset += dt.imgoffset;

	// Calculate length per sector
	int length = 1 << dt.size;

	// Open file
	Fileio fio;
	if (!fio.Open(path, Fileio::OpenMode::ReadWrite)) {
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
	memset(dt.changemap, 0x00, dt.sectors * sizeof(bool));
	dt.changed = false;
	return true;
}

bool DiskTrack::ReadSector(BYTE *buf, int sec) const
{
	assert((sec >= 0) && (sec < 0x100));

	LOGTRACE("%s reading sector: %d", __PRETTY_FUNCTION__,sec)
	// Error if not initialized
	if (!dt.init) {
		return false;
	}

	// // Error if the number of sectors exceeds the valid number
	if (sec >= dt.sectors) {
		return false;
	}

	// Copy
	assert(dt.buffer);
	assert((dt.sectors > 0) && (dt.sectors <= 0x100));
	memcpy(buf, &dt.buffer[(off_t)sec << dt.size], (off_t)1 << dt.size);

	// Success
	return true;
}

bool DiskTrack::WriteSector(const BYTE *buf, int sec)
{
	assert((sec >= 0) && (sec < 0x100));
	assert(!dt.raw);

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
	assert(dt.buffer);
	assert((dt.sectors > 0) && (dt.sectors <= 0x100));
	if (memcmp(buf, &dt.buffer[offset], length) == 0) {
		// Exit normally since it's attempting to write the same thing
		return true;
	}

	// Copy, change
	memcpy(&dt.buffer[offset], buf, length);
	dt.changemap[sec] = true;
	dt.changed = true;

	// Success
	return true;
}

//===========================================================================
//
//	Disk Cache
//
//===========================================================================

DiskCache::DiskCache(const Filepath& path, int size, uint32_t blocks, off_t imgoff)
	: sec_size(size), sec_blocks(blocks), imgoffset(imgoff)
{
	assert(blocks > 0);
	assert(imgoff >= 0);

	sec_path = path;
}

DiskCache::~DiskCache()
{
	// Clear the track
	Clear();
}

void DiskCache::SetRawMode(bool raw)
{
	// Configuration
	cd_raw = raw;
}

bool DiskCache::Save() const
{
	// Save track
	for (const cache_t& c : cache) {
		// Save if this is a valid track
		if (c.disktrk && !c.disktrk->Save(sec_path)) {
			return false;
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
	assert((index >= 0) && (index < CACHE_MAX));

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
	for (cache_t& c : cache) {
		if (c.disktrk) {
			delete c.disktrk;
			c.disktrk = nullptr;
		}
	}
}

bool DiskCache::ReadSector(BYTE *buf, uint32_t block)
{
	assert(sec_size != 0);

	// Update first
	UpdateSerialNumber();

	// Calculate track (fixed to 256 sectors/track)
	int track = block >> 8;

	// Get the track data
	const DiskTrack *disktrk = Assign(track);
	if (disktrk == nullptr) {
		return false;
	}

	// Read the track data to the cache
	return disktrk->ReadSector(buf, block & 0xff);
}

bool DiskCache::WriteSector(const BYTE *buf, uint32_t block)
{
	assert(sec_size != 0);

	// Update first
	UpdateSerialNumber();

	// Calculate track (fixed to 256 sectors/track)
	int track = block >> 8;

	// Get that track data
	DiskTrack *disktrk = Assign(track);
	if (disktrk == nullptr) {
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
	assert(sec_size != 0);
	assert(track >= 0);

	// First, check if it is already assigned
	for (cache_t& c : cache) {
		if (c.disktrk && c.disktrk->GetTrack() == track) {
			// Track match
			c.serial = serial;
			return c.disktrk;
		}
	}

	// Next, check for empty
	for (int i = 0; i < CACHE_MAX; i++) {
		if (!cache[i].disktrk) {
			// Try loading
			if (Load(i, track)) {
				// Success loading
				cache[i].serial = serial;
				return cache[i].disktrk;
			}

			// Load failed
			return nullptr;
		}
	}

	// Finally, find the youngest serial number and delete it

	// Set index 0 as candidate c
	DWORD s = cache[0].serial;
	int c = 0;

	// Compare candidate with serial and update to smaller one
	for (int i = 0; i < CACHE_MAX; i++) {
		assert(cache[i].disktrk);

		// Compare and update the existing serial
		if (cache[i].serial < s) {
			s = cache[i].serial;
			c = i;
		}
	}

	// Save this track
	if (!cache[c].disktrk->Save(sec_path)) {
		return nullptr;
	}

	// Delete this track
	DiskTrack *disktrk = cache[c].disktrk;
	cache[c].disktrk = nullptr;

	if (Load(c, track, disktrk)) {
		// Successful loading
		cache[c].serial = serial;
		return cache[c].disktrk;
	}

	// Load failed
	return nullptr;
}

//---------------------------------------------------------------------------
//
//	Load cache
//
//---------------------------------------------------------------------------
bool DiskCache::Load(int index, int track, DiskTrack *disktrk)
{
	assert((index >= 0) && (index < CACHE_MAX));
	assert(track >= 0);
	assert(!cache[index].disktrk);

	// Get the number of sectors on this track
	int sectors = sec_blocks - (track << 8);
	assert(sectors > 0);
	if (sectors > 0x100) {
		sectors = 0x100;
	}

	// Create a disk track
	if (disktrk == nullptr) {
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

	// Clear serial of all caches
	for (cache_t& c : cache) {
		c.serial = 0;
	}
}

