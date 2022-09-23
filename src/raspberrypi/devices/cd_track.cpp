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

#include "cd_track.h"
#include "fileio.h"

void CDTrack::Init(int track, uint32_t first, uint32_t last)
{
	assert(!valid);
	assert(track >= 1);
	assert(first < last);

	// Set and enable track number
	track_no = track;
	valid = true;

	// Remember LBA
	first_lba = first;
	last_lba = last;
}

void CDTrack::SetPath(bool cdda, const Filepath& path)
{
	assert(valid);

	// CD-DA or data
	audio = cdda;

	// Remember the path
	imgpath = path;
}

void CDTrack::GetPath(Filepath& path) const
{
	assert(valid);

	// Return the path (by reference)
	path = imgpath;
}

//---------------------------------------------------------------------------
//
//	Gets the start of LBA
//
//---------------------------------------------------------------------------
uint32_t CDTrack::GetFirst() const
{
	assert(valid);
	assert(first_lba < last_lba);

	return first_lba;
}

//---------------------------------------------------------------------------
//
//	Get the end of LBA
//
//---------------------------------------------------------------------------
uint32_t CDTrack::GetLast() const
{
	assert(valid);
	assert(first_lba < last_lba);

	return last_lba;
}

uint32_t CDTrack::GetBlocks() const
{
	assert(valid);
	assert(first_lba < last_lba);

	// Calculate from start LBA and end LBA
	return last_lba - first_lba + 1;
}

int CDTrack::GetTrackNo() const
{
	assert(valid);
	assert(track_no >= 1);

	return track_no;
}

//---------------------------------------------------------------------------
//
//	Is valid block
//
//---------------------------------------------------------------------------
bool CDTrack::IsValid(uint32_t lba) const
{
	// false if the track itself is invalid
	if (!valid) {
		return false;
	}

	// If the block is BEFORE the first block
	if (lba < first_lba) {
		return false;
	}

	// If the block is AFTER the last block
	if (last_lba < lba) {
		return false;
	}

	// This track is valid
	return true;
}

bool CDTrack::IsAudio() const
{
	assert(valid);

	return audio;
}
