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

#include <cstdlib>
#include <span>
#include <vector>
#include <string>

using namespace std;

class DiskTrack
{
	 struct {
		int track;							// Track Number
		int size;							// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
		int sectors;						// Number of sectors(<0x100)
		uint32_t length;					// Data buffer length
		uint8_t *buffer;						// Data buffer
		bool init;							// Is it initilized?
		bool changed;						// Changed flag
		vector<bool> changemap;				// Changed map
		bool raw;							// RAW mode flag
		off_t imgoffset;					// Offset to actual data
	} dt = {};

public:

	DiskTrack() = default;
	~DiskTrack();
	DiskTrack(DiskTrack&) = delete;
	DiskTrack& operator=(const DiskTrack&) = delete;

private:

	friend class DiskCache;

	void Init(int track, int size, int sectors, bool raw = false, off_t imgoff = 0);
	bool Load(const string& path);
	bool Save(const string& path);

	// Read / Write
	bool ReadSector(span<uint8_t>, int) const;				// Sector Read
	bool WriteSector(span<const uint8_t> buf, int);			// Sector Write

	int GetTrack() const		{ return dt.track; }		// Get track
};
