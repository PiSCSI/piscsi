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
//---------------------------------------------------------------------------

#include "os.h"
#include "log.h"
#include "fileio.h"
#include "disk_track.h"
#include "disk_cache.h"

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

