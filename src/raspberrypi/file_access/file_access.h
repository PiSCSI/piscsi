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

#include "filepath.h"

class FileAccess
{
public:
	FileAccess(const Filepath& path, int size, uint32_t blocks, off_t imgoff = 0);
	virtual ~FileAccess();

	void SetRawMode(bool raw) { cd_raw = raw; };		// CD-ROM raw mode setting

	// Access
	virtual bool Save() = 0;							// Save and release all
	virtual bool ReadSector(BYTE *buf, int block) = 0;				// Sector Read
	virtual bool WriteSector(const BYTE *buf, int block) = 0;			// Sector Write
	virtual bool GetCache(int index, int& track, DWORD& serial) const = 0;	// Get cache information

protected:
	bool cd_raw = false;
	DWORD serial;								// Last serial number
	Filepath sec_path;							// Path

	int sec_size;								// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
	int sec_blocks;								// Blocks per sector
	off_t imgoffset;							// Offset to actual data

	
	off_t GetTrackOffset(int block);
	off_t GetSectorOffset(int block);
};

