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

#include "generated/piscsi_interface.pb.h"
#include <span>
#include <array>
#include <memory>
#include <string>

using namespace std;
using namespace piscsi_interface;

class DiskCache
{
	// Number of tracks to cache
	static const int CACHE_MAX = 16;

	uint64_t read_error_count = 0;
	uint64_t write_error_count = 0;
	uint64_t cache_miss_read_count = 0;
	uint64_t cache_miss_write_count = 0;

	inline static const string READ_ERROR_COUNT = "read_error_count";
	inline static const string WRITE_ERROR_COUNT = "write_error_count";
	inline static const string CACHE_MISS_READ_COUNT = "cache_miss_read_count";
	inline static const string CACHE_MISS_WRITE_COUNT = "cache_miss_write_count";

public:

	// Internal data definition
	using cache_t = struct {
		shared_ptr<DiskTrack> disktrk;	// Disk Track
		uint32_t serial;				// Serial
	};

	DiskCache(const string&, int, uint32_t, off_t = 0);
	~DiskCache() = default;

	void SetRawMode(bool b) { cd_raw = b; }		// CD-ROM raw mode setting

	bool Save();							// Save and release all
	bool ReadSector(span<uint8_t>, uint32_t);			// Sector Read
	bool WriteSector(span<const uint8_t>, uint32_t);	// Sector Write

	vector<PbStatistics> GetStatistics(bool) const;

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

