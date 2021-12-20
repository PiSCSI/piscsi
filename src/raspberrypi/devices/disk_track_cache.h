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

#include "../rascsi.h"
#include "filepath.h"

// Number of tracks to cache
#define CacheMax 16

class DiskTrack
{
private:
	 struct {
		int track;							// Track Number
		int size;							// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
		int sectors;							// Number of sectors(<0x100)
		DWORD length;							// Data buffer length
		BYTE *buffer;							// Data buffer
		BOOL init;							// Is it initilized?
		BOOL changed;							// Changed flag
		DWORD maplen;							// Changed map length
		BOOL *changemap;						// Changed map
		BOOL raw;							// RAW mode flag
		off_t imgoffset;						// Offset to actual data
	} dt;

public:
	DiskTrack();
	~DiskTrack();

	void Init(int track, int size, int sectors, BOOL raw = FALSE, off_t imgoff = 0);
	bool Load(const Filepath& path);
	bool Save(const Filepath& path);

	// Read / Write
	bool ReadSector(BYTE *buf, int sec) const;				// Sector Read
	bool WriteSector(const BYTE *buf, int sec);				// Sector Write

	int GetTrack() const		{ return dt.track; }		// Get track
};

class DiskCache
{
public:
	// Internal data definition
	typedef struct {
		DiskTrack *disktrk;						// Disk Track
		DWORD serial;							// Serial
	} cache_t;

public:
	DiskCache(const Filepath& path, int size, uint32_t blocks, off_t imgoff = 0);
	~DiskCache();

	void SetRawMode(BOOL raw);					// CD-ROM raw mode setting

	// Access
	bool Save();							// Save and release all
	bool ReadSector(BYTE *buf, int block);				// Sector Read
	bool WriteSector(const BYTE *buf, int block);			// Sector Write
	bool GetCache(int index, int& track, DWORD& serial) const;	// Get cache information

private:
	// Internal Management
	void Clear();							// Clear all tracks
	DiskTrack* Assign(int track);					// Load track
	bool Load(int index, int track, DiskTrack *disktrk = NULL);	// Load track
	void UpdateSerialNumber();							// Update serial number

	// Internal data
	cache_t cache[CacheMax];						// Cache management
	DWORD serial;								// Last serial number
	Filepath sec_path;							// Path
	int sec_size;								// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
	int sec_blocks;								// Blocks per sector
	BOOL cd_raw;								// CD-ROM RAW mode
	off_t imgoffset;							// Offset to actual data
};

