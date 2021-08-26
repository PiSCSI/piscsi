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
//	[ DiskTrack and DiskCache]
//
//---------------------------------------------------------------------------

#pragma once

#include "../rascsi.h"
#include "log.h"
#include "filepath.h"

class DiskTrack
{
public:
	// Internal data definition
	typedef struct {
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
	} disktrk_t;

public:
	// Basic Functions
	DiskTrack();								// Constructor
	virtual ~DiskTrack();							// Destructor
	void Init(int track, int size, int sectors, BOOL raw = FALSE, off_t imgoff = 0);// Initialization
	BOOL Load(const Filepath& path);				// Load
	BOOL Save(const Filepath& path);				// Save

	// Read / Write
	BOOL Read(BYTE *buf, int sec) const;				// Sector Read
	BOOL Write(const BYTE *buf, int sec);				// Sector Write

	// Other
	int GetTrack() const		{ return dt.track; }		// Get track
	BOOL IsChanged() const		{ return dt.changed; }		// Changed flag check

private:
	// Internal data
	disktrk_t dt;								// Internal data
};

//===========================================================================
//
//	Disk Cache
//
//===========================================================================
class DiskCache
{
public:
	// Internal data definition
	typedef struct {
		DiskTrack *disktrk;						// Disk Track
		DWORD serial;							// Serial
	} cache_t;

	// Number of caches
	enum {
		CacheMax = 16							// Number of tracks to cache
	};

public:
	// Basic Functions
	DiskCache(const Filepath& path, int size, int blocks,off_t imgoff = 0);// Constructor
	virtual ~DiskCache();							// Destructor
	void SetRawMode(BOOL raw);					// CD-ROM raw mode setting

	// Access
	BOOL Save();							// Save and release all
	BOOL Read(BYTE *buf, int block);				// Sector Read
	BOOL Write(const BYTE *buf, int block);			// Sector Write
	BOOL GetCache(int index, int& track, DWORD& serial) const;	// Get cache information

private:
	// Internal Management
	void Clear();							// Clear all tracks
	DiskTrack* Assign(int track);					// Load track
	BOOL Load(int index, int track, DiskTrack *disktrk = NULL);	// Load track
	void Update();							// Update serial number

	// Internal data
	cache_t cache[CacheMax];						// Cache management
	DWORD serial;								// Last serial number
	Filepath sec_path;							// Path
	int sec_size;								// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
	int sec_blocks;								// Blocks per sector
	BOOL cd_raw;								// CD-ROM RAW mode
	off_t imgoffset;							// Offset to actual data
};

