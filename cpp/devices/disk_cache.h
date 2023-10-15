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

#include <span>
#include <array>
#include <memory>
#include <string>

using namespace std;

class DiskCache
{
	// Number of tracks to cache
	static const int CACHE_MAX = 16;

public:

	// Internal data definition
	using cache_t = struct {
		shared_ptr<DiskTrack> disktrk;	// Disk Track
		uint32_t serial;				// Serial
	};

	DiskCache(const string&, int, uint32_t, off_t = 0);
	~DiskCache() = default;

	void SetRawMode(bool b) { cd_raw = b; }		// CD-ROM raw mode setting

	// Access
	bool Save() const;							// Save and release all
	bool ReadSector(span<uint8_t>, uint32_t);			// Sector Read
	bool WriteSector(span<const uint8_t>, uint32_t);	// Sector Write

private:

	// Internal Management
	shared_ptr<DiskTrack> Assign(int);
	shared_ptr<DiskTrack> GetTrack(uint32_t);
	bool Load(int index, int track, shared_ptr<DiskTrack>);
	void UpdateSerialNumber();

	// Internal data
	array<cache_t, CACHE_MAX> cache = {};		// Cache management
	uint32_t serial = 0;						// Last serial number
	string sec_path;							// Path
	int sec_size;								// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
	int sec_blocks;								// Blocks per sector
	bool cd_raw = false;						// CD-ROM RAW mode
	off_t imgoffset;							// Offset to actual data
};

