//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	XM6i
//	Copyright (C) 2010-2015 isaki@NetBSD.org
//
//  	Imported sava's Anex86/T98Next image and MO format support patch.
//  	Comments translated to english by akuker.
//
//---------------------------------------------------------------------------

#pragma once

#include "filepath.h"

// Number of tracks to cache
static const int CACHE_MAX = 16;

class DiskCache
{
public:

	// Internal data definition
	using cache_t = struct {
		DiskTrack *disktrk;					// Disk Track
		uint32_t serial;					// Serial
	};

	DiskCache(const Filepath& path, int size, uint32_t blocks, off_t imgoff = 0);
	~DiskCache();
	DiskCache(DiskCache&) = delete;
	DiskCache& operator=(const DiskCache&) = delete;

	void SetRawMode(bool);					// CD-ROM raw mode setting

	// Access
	bool Save() const;							// Save and release all
	bool ReadSector(BYTE *buf, uint32_t block);				// Sector Read
	bool WriteSector(const BYTE *buf, uint32_t block);			// Sector Write
	bool GetCache(int index, int& track, uint32_t& serial) const;	// Get cache information

private:

	// Internal Management
	void Clear();							// Clear all tracks
	DiskTrack* Assign(int track);					// Load track
	bool Load(int index, int track, DiskTrack *disktrk = nullptr);	// Load track
	void UpdateSerialNumber();							// Update serial number

	// Internal data
	cache_t cache[CACHE_MAX] = {};				// Cache management
	uint32_t serial = 0;						// Last serial number
	Filepath sec_path;							// Path
	int sec_size;								// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
	int sec_blocks;								// Blocks per sector
	bool cd_raw = false;						// CD-ROM RAW mode
	off_t imgoffset;							// Offset to actual data
};

