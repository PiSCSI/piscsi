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
//	[ DiskTrack and DiskCache ]
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include "disk_image/disk_image_handle.h"

using namespace std;

// Number of tracks to cache
#define CacheMax 16

class DiskTrack
{
private:
	 struct {
		int track;							// Track Number
		int size;							// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
		int sectors;							// Number of sectors(<0x100)
		uint32_t length;							// Data buffer length
		uint8_t *buffer;							// Data buffer
		bool init;							// Is it initilized?
		bool changed;							// Changed flag
		uint32_t maplen;							// Changed map length
		bool *changemap;						// Changed map
		bool raw;							// RAW mode flag
		off_t imgoffset;						// Offset to actual data
	} dt;

public:
	DiskTrack();
	~DiskTrack();

private:
	friend class DiskCache;

	void Init(int track, int size, int sectors, bool raw = false, off_t imgoff = 0) ;
	bool Load(const string& path) ;
	bool Save(const string& path) ;

	// Read / Write
	bool ReadSector(vector<uint8_t>& buf, int sec) const;				// Sector Read
	bool WriteSector(const vector<uint8_t>& buf, int sec);				// Sector Write

	int GetTrack() const		{ return dt.track; }		// Get track
};

class DiskCache : public DiskImageHandle
{
public:
	// Internal data definition
	typedef struct {
		DiskTrack *disktrk;						// Disk Track
		uint32_t serial;							// Serial
	} cache_t;

public:
	DiskCache(const string& path, int size, uint32_t blocks, off_t imgoff = 0);
	~DiskCache();

	// Access
	bool Save() override;							// Save and release all
	bool ReadSector(vector<uint8_t>& buf, int block) override;				// Sector Read
	bool WriteSector(const vector<uint8_t>& buf, int block) override;			// Sector Write
	bool GetCache(int index, int& track, uint32_t& serial) const override;	// Get cache information

private:
	// Internal Management
	void Clear();							// Clear all tracks
	DiskTrack* Assign(int track);					// Load track
	bool Load(int index, int track, DiskTrack *disktrk = NULL);	// Load track
	void UpdateSerialNumber();							// Update serial number

	// Internal data
	cache_t cache[CacheMax];						// Cache management
};

