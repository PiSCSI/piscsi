//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License. 
//	See LICENSE file in the project root folder.
//
//---------------------------------------------------------------------------

#pragma once

#include "filepath.h"

class CDTrack final
{
public:

	CDTrack() = default;
	~CDTrack() = default;
	CDTrack(CDTrack&) = delete;
	CDTrack& operator=(const CDTrack&) = delete;

	void Init(int track, DWORD first, DWORD last);

	// Properties
	void SetPath(bool cdda, const Filepath& path);		// Set the path
	void GetPath(Filepath& path) const;		// Get the path
	uint32_t GetFirst() const;				// Get the start LBA
	uint32_t GetLast() const;				// Get the last LBA
	uint32_t GetBlocks() const;				// Get the number of blocks
	int GetTrackNo() const;					// Get the track number
	bool IsValid(DWORD lba) const;			// Is this a valid LBA?
	bool IsAudio() const;					// Is this an audio track?

private:
	bool valid = false;						// Valid track
	int track_no = -1;						// Track number
	uint32_t first_lba = 0;					// First LBA
	uint32_t last_lba = 0;					// Last LBA
	bool audio = false;						// Audio track flag
	Filepath imgpath;						// Image file path
};
