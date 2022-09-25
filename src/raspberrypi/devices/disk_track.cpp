//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	XM6i
//	Copyright (C) 2010-2015 isaki@NetBSD.org
//	Copyright (C) 2010 Y.Sugahara
//
//	Imported sava's Anex86/T98Next image and MO format support patch.
//  	Comments translated to english by akuker.
//
//---------------------------------------------------------------------------

#include "log.h"
#include "fileio.h"
#include "disk_track.h"

DiskTrack::~DiskTrack()
{
	// Release memory, but do not save automatically
	if (dt.buffer) {
		free(dt.buffer);
	}
}

void DiskTrack::Init(int track, int size, int sectors, bool raw, off_t imgoff)
{
	assert(track >= 0);
	assert((sectors > 0) && (sectors <= 0x100));
	assert(imgoff >= 0);

	// Set Parameters
	dt.track = track;
	dt.size = size;
	dt.sectors = sectors;
	dt.raw = raw;

	// Not initialized (needs to be loaded)
	dt.init = false;

	// Not Changed
	dt.changed = false;

	// Offset to actual data
	dt.imgoffset = imgoff;
}

bool DiskTrack::Load(const Filepath& path)
{
	// Not needed if already loaded
	if (dt.init) {
		assert(dt.buffer);
		return true;
	}

	// Calculate offset (previous tracks are considered to hold 256 sectors)
	off_t offset = ((off_t)dt.track << 8);
	if (dt.raw) {
		assert(dt.size == 11);
		offset *= 0x930;
		offset += 0x10;
	} else {
		offset <<= dt.size;
	}

	// Add offset to real image
	offset += dt.imgoffset;

	// Calculate length (data size of this track)
	int length = dt.sectors << dt.size;

	// Allocate buffer memory
	assert((dt.sectors > 0) && (dt.sectors <= 0x100));

	if (dt.buffer == nullptr) {
		if (posix_memalign((void **)&dt.buffer, 512, ((length + 511) / 512) * 512)) {
			LOGWARN("%s posix_memalign failed", __PRETTY_FUNCTION__)
		}
		dt.length = length;
	}

	if (dt.buffer == nullptr) {
		return false;
	}

	// Reallocate if the buffer length is different
	if (dt.length != (DWORD)length) {
		free(dt.buffer);
		if (posix_memalign((void **)&dt.buffer, 512, ((length + 511) / 512) * 512)) {
			LOGWARN("%s posix_memalign failed", __PRETTY_FUNCTION__)
        }
		dt.length = length;
	}

	// Resize and clear changemap
	dt.changemap.resize(dt.sectors);
	fill(dt.changemap.begin(), dt.changemap.end(), false);

	// Read from File
	Fileio fio;
	if (!fio.OpenDIO(path, Fileio::OpenMode::ReadOnly)) {
		return false;
	}

	if (dt.raw) {
		// Split Reading
		for (int i = 0; i < dt.sectors; i++) {
			// Seek
			if (!fio.Seek(offset)) {
				fio.Close();
				return false;
			}

			// Read
			if (!fio.Read(&dt.buffer[i << dt.size], 1 << dt.size)) {
				fio.Close();
				return false;
			}

			// Next offset
			offset += 0x930;
		}
	} else {
		// Continuous reading
		if (!fio.Seek(offset)) {
			fio.Close();
			return false;
		}
		if (!fio.Read(dt.buffer, length)) {
			fio.Close();
			return false;
		}
	}
	fio.Close();

	// Set a flag and end normally
	dt.init = true;
	dt.changed = false;
	return true;
}

bool DiskTrack::Save(const Filepath& path)
{
	// Not needed if not initialized
	if (!dt.init) {
		return true;
	}

	// Not needed unless changed
	if (!dt.changed) {
		return true;
	}

	// Need to write
	assert(dt.buffer);
	assert((dt.sectors > 0) && (dt.sectors <= 0x100));

	// Writing in RAW mode is not allowed
	assert(!dt.raw);

	// Calculate offset (previous tracks are considered to hold 256 sectors)
	off_t offset = ((off_t)dt.track << 8);
	offset <<= dt.size;

	// Add offset to real image
	offset += dt.imgoffset;

	// Calculate length per sector
	int length = 1 << dt.size;

	// Open file
	Fileio fio;
	if (!fio.Open(path, Fileio::OpenMode::ReadWrite)) {
		return false;
	}

	// Partial write loop
	int total;
	for (int i = 0; i < dt.sectors;) {
		// If changed
		if (dt.changemap[i]) {
			// Initialize write size
			total = 0;

			// Seek
			if (!fio.Seek(offset + ((off_t)i << dt.size))) {
				fio.Close();
				return false;
			}

			// Consectutive sector length
			int j;
			for (j = i; j < dt.sectors; j++) {
				// end when interrupted
				if (!dt.changemap[j]) {
					break;
				}

				// Add one sector
				total += length;
			}

			// Write
			if (!fio.Write(&dt.buffer[i << dt.size], total)) {
				fio.Close();
				return false;
			}

			// To unmodified sector
			i = j;
		} else {
			// Next Sector
			i++;
		}
	}

	fio.Close();

	// Drop the change flag and exit
	fill(dt.changemap.begin(), dt.changemap.end(), false);
	dt.changed = false;

	return true;
}

bool DiskTrack::ReadSector(vector<BYTE>& buf, int sec) const
{
	assert(sec >= 0 && sec < 0x100);

	LOGTRACE("%s reading sector: %d", __PRETTY_FUNCTION__,sec)

	// Error if not initialized
	if (!dt.init) {
		return false;
	}

	// // Error if the number of sectors exceeds the valid number
	if (sec >= dt.sectors) {
		return false;
	}

	// Copy
	assert(dt.buffer);
	assert((dt.sectors > 0) && (dt.sectors <= 0x100));
	memcpy(buf.data(), &dt.buffer[(off_t)sec << dt.size], (off_t)1 << dt.size);

	// Success
	return true;
}

bool DiskTrack::WriteSector(const vector<BYTE>& buf, int sec)
{
	assert((sec >= 0) && (sec < 0x100));
	assert(!dt.raw);

	// Error if not initialized
	if (!dt.init) {
		return false;
	}

	// // Error if the number of sectors exceeds the valid number
	if (sec >= dt.sectors) {
		return false;
	}

	// Calculate offset and length
	int offset = sec << dt.size;
	int length = 1 << dt.size;

	// Compare
	assert(dt.buffer);
	assert((dt.sectors > 0) && (dt.sectors <= 0x100));
	if (memcmp(buf.data(), &dt.buffer[offset], length) == 0) {
		// Exit normally since it's attempting to write the same thing
		return true;
	}

	// Copy, change
	memcpy(&dt.buffer[offset], buf.data(), length);
	dt.changemap[sec] = true;
	dt.changed = true;

	// Success
	return true;
}
