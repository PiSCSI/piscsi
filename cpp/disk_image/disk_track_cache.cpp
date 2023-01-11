//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  Copyright (C) 2022-2023 akuker
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

#include <fstream>
#include "shared/log.h"
#include "disk_track_cache.h"

using namespace std;

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
	dt.raw = false;
	dt.init = false;
	dt.changed = false;
	dt.length = 0;
	dt.buffer = nullptr;
	dt.maplen = 0;
	dt.changemap = nullptr;
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

bool DiskTrack::Load(const string& path)
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
	if (dt.length != (uint32_t)length) {
		free(dt.buffer);
		if (posix_memalign((void **)&dt.buffer, 512, ((length + 511) / 512) * 512)) {
                  LOGWARN("%s posix_memalign failed", __PRETTY_FUNCTION__);  
                }
		dt.length = length;
	}

	// Reserve change map memory
	if (dt.changemap == NULL) {
		dt.changemap = (bool *)malloc(dt.sectors * sizeof(bool));
		dt.maplen = dt.sectors;
	}

	if (!dt.changemap) {
		return false;
	}

	// Reallocate if the buffer length is different
	if (dt.maplen != (uint32_t)dt.sectors) {
		free(dt.changemap);
		dt.changemap = (bool *)malloc(dt.sectors * sizeof(bool));
		dt.maplen = dt.sectors;
	}

	// Clear changemap
	memset(dt.changemap, 0x00, dt.sectors * sizeof(bool));

	// Read from File
	fstream fio;
	fio.open(path.c_str(),ios::in);
	if(!fio.is_open()) {
		return false;
	}
	if (dt.raw) {
		// Split Reading
		for (int i = 0; i < dt.sectors; i++) {
			// Seek
			if (!fio.seekg(offset)) {
				fio.close();
				return false;
			}

			// Read
			if (!fio.read((char *)&dt.buffer[i << dt.size], 1 << dt.size)) {
				fio.close();
				return false;
			}

			// Next offset
			offset += 0x930;
		}
	} else {
		// Continuous reading
		if (!fio.seekg(offset)) {
			fio.close();
			return false;
		}
		if (!fio.read((char*)dt.buffer, length)) {
			fio.close();
			return false;
		}
	}
	fio.close();

	// Set a flag and end normally
	dt.init = true;
	dt.changed = false;
	return true;
}

bool DiskTrack::Save(const string& path)
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
	fstream fio;
	fio.open(path, ios::in | ios::out);
	if (!fio.is_open()) {
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
			if (!fio.seekg(offset + ((off_t)i << dt.size))) {
				fio.close();
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
			if (!fio.write((char*)&dt.buffer[i << dt.size], total)) {
				fio.close();
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
	fio.close();

	// Drop the change flag and exit
	memset(dt.changemap, 0x00, dt.sectors * sizeof(bool));
	dt.changed = false;
	return true;
}

bool DiskTrack::ReadSector(vector<uint8_t>& buf, int sec) const
{
	assert((sec >= 0) & (sec < 0x100));

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
	memcpy(buf.data(), &dt.buffer[(off_t)sec << dt.size], (off_t)1 << dt.size);

	// Success
	return true;
}

bool DiskTrack::WriteSector(const vector<uint8_t>& buf, int sec)
{
	assert((sec >= 0) & (sec < 0x100));
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
	if (memcmp(buf.data(), &dt.buffer[offset], length) == 0) {
		// Exit normally since it's attempting to write the same thing
		return true;
	}

	// Copy, change
	memcpy(&dt.buffer[offset], buf.data(), length);
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

DiskCache::DiskCache(const string& path, int size, uint32_t blocks, off_t imgoff) : DiskImageHandle(path, size, blocks, imgoff)
{
	assert(blocks > 0);
	assert(imgoff >= 0);

	// Cache work
	for (int i = 0; i < CacheMax; i++) {
		cache[i].disktrk = NULL;
		cache[i].serial = 0;
	}

}

DiskCache::~DiskCache()
{
	// Clear the track
	Clear();
}

bool DiskCache::Save()
{
	// Save track
	for (int i = 0; i < CacheMax; i++) {
		// Is it a valid track?
		if (cache[i].disktrk) {
			// Save
			if (!cache[i].disktrk->Save(GetPath())) {
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
bool DiskCache::GetCache(int index, int& track, uint32_t& aserial) const
{
	assert((index >= 0) && (index < CacheMax));

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

bool DiskCache::ReadSector(vector<uint8_t>& buf, int block)
{
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

bool DiskCache::WriteSector(const vector<uint8_t>& buf, int block)
{
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
	assert(track >= 0);

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
	uint32_t s = cache[0].serial;
	int c = 0;

	// Compare candidate with serial and update to smaller one
	for (int i = 0; i < CacheMax; i++) {
		assert(cache[i].disktrk);

		// Compare and update the existing serial
		if (cache[i].serial < s) {
			s = cache[i].serial;
			c = i;
		}
	}

	// Save this track
	if (!cache[c].disktrk->Save(GetPath())) {
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
	assert((index >= 0) && (index < CacheMax));
	assert(track >= 0);
	assert(!cache[index].disktrk);

	// Get the number of sectors on this track
	int sectors = GetBlocksPerSector() - (track << 8);
	assert(sectors > 0);
	if (sectors > 0x100) {
		sectors = 0x100;
	}

	// Create a disk track
	if (disktrk == NULL) {
		disktrk = new DiskTrack();
	}

	// Initialize disk track
	disktrk->Init(track, GetSectorSize(), GetBlocksPerSector(), GetRawMode(), GetImgOffset());

	// Try loading
	if (!disktrk->Load(GetPath())) {
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

