//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  Copyright (C) akuker
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ SCSI Hard Disk for Apple Macintosh ]
//
//---------------------------------------------------------------------------

#include "xm6.h"
#include "scsicd.h"
#include "fileio.h"

//===========================================================================
//
//	CD Track
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CDTrack::CDTrack(SCSICD *scsicd)
{
	ASSERT(scsicd);

	// Set parent CD-ROM device
	cdrom = scsicd;

	// Track defaults to disabled
	valid = FALSE;

	// Initialize other data
	track_no = -1;
	first_lba = 0;
	last_lba = 0;
	audio = FALSE;
	raw = FALSE;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
CDTrack::~CDTrack()
{
}

//---------------------------------------------------------------------------
//
//	Init
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::Init(int track, DWORD first, DWORD last)
{
	ASSERT(this);
	ASSERT(!valid);
	ASSERT(track >= 1);
	ASSERT(first < last);

	// Set and enable track number
	track_no = track;
	valid = TRUE;

	// Remember LBA
	first_lba = first;
	last_lba = last;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Set Path
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::SetPath(BOOL cdda, const Filepath& path)
{
	ASSERT(this);
	ASSERT(valid);

	// CD-DA or data
	audio = cdda;

	// Remember the path
	imgpath = path;
}

//---------------------------------------------------------------------------
//
//	Get Path
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::GetPath(Filepath& path) const
{
	ASSERT(this);
	ASSERT(valid);

	// Return the path (by reference)
	path = imgpath;
}

//---------------------------------------------------------------------------
//
//	Add Index
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::AddIndex(int index, DWORD lba)
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(index > 0);
	ASSERT(first_lba <= lba);
	ASSERT(lba <= last_lba);

	// Currently does not support indexes
	ASSERT(FALSE);
}

//---------------------------------------------------------------------------
//
//	Gets the start of LBA
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetFirst() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	return first_lba;
}

//---------------------------------------------------------------------------
//
//	Get the end of LBA
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetLast() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	return last_lba;
}

//---------------------------------------------------------------------------
//
//	Get the number of blocks
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetBlocks() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	// Calculate from start LBA and end LBA
	return (DWORD)(last_lba - first_lba + 1);
}

//---------------------------------------------------------------------------
//
//	Get track number
//
//---------------------------------------------------------------------------
int FASTCALL CDTrack::GetTrackNo() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(track_no >= 1);

	return track_no;
}

//---------------------------------------------------------------------------
//
//	Is valid block
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::IsValid(DWORD lba) const
{
	ASSERT(this);

	// FALSE if the track itself is invalid
	if (!valid) {
		return FALSE;
	}

	// If the block is BEFORE the first block
	if (lba < first_lba) {
		return FALSE;
	}

	// If the block is AFTER the last block
	if (last_lba < lba) {
		return FALSE;
	}

	// This track is valid
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Is audio track
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::IsAudio() const
{
	ASSERT(this);
	ASSERT(valid);

	return audio;
}

//===========================================================================
//
//	CD-DA Buffer
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CDDABuf::CDDABuf()
{
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
CDDABuf::~CDDABuf()
{
}

//===========================================================================
//
//	SCSI CD-ROM
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSICD::SCSICD() : Disk()
{
	int i;

	// SCSI CD-ROM
	disk.id = MAKEID('S', 'C', 'C', 'D');

	// removable, write protected
	disk.removable = TRUE;
	disk.writep = TRUE;

	// NOT in raw format
	rawfile = FALSE;

	// Frame initialization
	frame = 0;

	// Track initialization
	for (i = 0; i < TrackMax; i++) {
		track[i] = NULL;
	}
	tracks = 0;
	dataindex = -1;
	audioindex = -1;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SCSICD::~SCSICD()
{
	// Clear track
	ClearTrack();
}

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::Load(Fileio *fio, int ver)
{
	DWORD sz;
	disk_t buf;
	DWORD padding;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);

	// Prior to version 2.03, the disk was not saved
	if (ver <= 0x0202) {
		return TRUE;
	}

	// load size, match
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != 52) {
		return FALSE;
	}

	// load into buffer
	PROP_IMPORT(fio, buf.id);
	PROP_IMPORT(fio, buf.ready);
	PROP_IMPORT(fio, buf.writep);
	PROP_IMPORT(fio, buf.readonly);
	PROP_IMPORT(fio, buf.removable);
	PROP_IMPORT(fio, buf.lock);
	PROP_IMPORT(fio, buf.attn);
	PROP_IMPORT(fio, buf.reset);
	PROP_IMPORT(fio, buf.size);
	PROP_IMPORT(fio, buf.blocks);
	PROP_IMPORT(fio, buf.lun);
	PROP_IMPORT(fio, buf.code);
	PROP_IMPORT(fio, padding);

	// Load path
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// Always eject
	Eject(TRUE);

	// move only if IDs match
	if (disk.id != buf.id) {
		// It was not a CD-ROM when saving. Maintain eject status
		return TRUE;
	}

	// Try to reopen
	if (!Open(path, FALSE)) {
		// Cannot reopen. Maintain eject status
		return TRUE;
	}

	// Disk cache is created in Open. Move property only
	if (!disk.readonly) {
		disk.writep = buf.writep;
	}
	disk.lock = buf.lock;
	disk.attn = buf.attn;
	disk.reset = buf.reset;
	disk.lun = buf.lun;
	disk.code = buf.code;

	// Discard the disk cache again
	if (disk.dcache) {
		delete disk.dcache;
		disk.dcache = NULL;
	}
	disk.dcache = NULL;

	// Tentative
	disk.blocks = track[0]->GetBlocks();
	if (disk.blocks > 0) {
		// Recreate the disk cache
		track[0]->GetPath(path);
		disk.dcache = new DiskCache(path, disk.size, disk.blocks);
		disk.dcache->SetRawMode(rawfile);

		// Reset data index
		dataindex = 0;
	}

	return TRUE;
}
#endif	// RASCSI

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::Open(const Filepath& path, BOOL attn)
{
	Fileio fio;
	off64_t size;
	TCHAR file[5];

	ASSERT(this);
	ASSERT(!disk.ready);

	// Initialization, track clear
	disk.blocks = 0;
	rawfile = FALSE;
	ClearTrack();

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Close and transfer for physical CD access
	if (path.GetPath()[0] == _T('\\')) {
		// Close
		fio.Close();

		// Open physical CD
		if (!OpenPhysical(path)) {
			return FALSE;
		}
	} else {
		// Get file size
        size = fio.GetFileSize();
		if (size <= 4) {
			fio.Close();
			return FALSE;
		}

		// Judge whether it is a CUE sheet or an ISO file
		fio.Read(file, 4);
		file[4] = '\0';
		fio.Close();

		// If it starts with FILE, consider it as a CUE sheet
		if (xstrncasecmp(file, _T("FILE"), 4) == 0) {
			// Open as CUE
			if (!OpenCue(path)) {
				return FALSE;
			}
		} else {
			// Open as ISO
			if (!OpenIso(path)) {
				return FALSE;
			}
		}
	}

	// Successful opening
	ASSERT(disk.blocks > 0);
	disk.size = 11;

	// Call the base class
	Disk::Open(path);

	// Set RAW flag
	ASSERT(disk.dcache);
	disk.dcache->SetRawMode(rawfile);

	// Since it is a ROM media, writing is not possible
	disk.writep = TRUE;

	// Attention if ready
	if (disk.ready && attn) {
		disk.attn = TRUE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Open (CUE)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::OpenCue(const Filepath& /*path*/)
{
	ASSERT(this);

	// Always fail
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	オープン(ISO)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::OpenIso(const Filepath& path)
{
	Fileio fio;
	off64_t size;
	BYTE header[12];
	BYTE sync[12];

	ASSERT(this);

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();
	if (size < 0x800) {
		fio.Close();
		return FALSE;
	}

	// Read the first 12 bytes and close
	if (!fio.Read(header, sizeof(header))) {
		fio.Close();
		return FALSE;
	}

	// Check if it is RAW format
	memset(sync, 0xff, sizeof(sync));
	sync[0] = 0x00;
	sync[11] = 0x00;
	rawfile = FALSE;
	if (memcmp(header, sync, sizeof(sync)) == 0) {
		// 00,FFx10,00, so it is presumed to be RAW format
		if (!fio.Read(header, 4)) {
			fio.Close();
			return FALSE;
		}

		// Supports MODE1/2048 or MODE1/2352 only
		if (header[3] != 0x01) {
			// Different mode
			fio.Close();
			return FALSE;
		}

		// Set to RAW file
		rawfile = TRUE;
	}
	fio.Close();

	if (rawfile) {
		// Size must be a multiple of 2536 and less than 700MB
		if (size % 0x930) {
			return FALSE;
		}
		if (size > 912579600) {
			return FALSE;
		}

		// Set the number of blocks
		disk.blocks = (DWORD)(size / 0x930);
	} else {
		// Size must be a multiple of 2048 and less than 700MB
		if (size & 0x7ff) {
			return FALSE;
		}
		if (size > 0x2bed5000) {
			return FALSE;
		}

		// Set the number of blocks
		disk.blocks = (DWORD)(size >> 11);
	}

	// Create only one data track
	ASSERT(!track[0]);
	track[0] = new CDTrack(this);
	track[0]->Init(1, 0, disk.blocks - 1);
	track[0]->SetPath(FALSE, path);
	tracks = 1;
	dataindex = 0;

	// Successful opening
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Open (Physical)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::OpenPhysical(const Filepath& path)
{
	Fileio fio;
	off64_t size;

	ASSERT(this);

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get size
	size = fio.GetFileSize();
	if (size < 0x800) {
		fio.Close();
		return FALSE;
	}

	// Close
	fio.Close();

	// Size must be a multiple of 2048 and less than 700MB
	if (size & 0x7ff) {
		return FALSE;
	}
	if (size > 0x2bed5000) {
		return FALSE;
	}

	// Set the number of blocks
	disk.blocks = (DWORD)(size >> 11);

	// Create only one data track
	ASSERT(!track[0]);
	track[0] = new CDTrack(this);
	track[0]->Init(1, 0, disk.blocks - 1);
	track[0]->SetPath(FALSE, path);
	tracks = 1;
	dataindex = 0;

	// Successful opening
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	char rev[32];
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Basic data
	// buf[0] ... CD-ROM Device
	// buf[1] ... Removable
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[0] = 0x05;

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buf[0] = 0x7f;
	}

	buf[1] = 0x80;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 36 - 5;	// Required

	// Fill with blanks
	memset(&buf[8], 0x20, buf[4] - 3);

	// Vendor name
	memcpy(&buf[8], BENDER_SIGNATURE, strlen(BENDER_SIGNATURE));

	// Product name
	memcpy(&buf[16], "CD-ROM CDU-55S", 14);

	// Revision (XM6 version number)
	sprintf(rev, "0%01d%01d%01d",
				(int)major, (int)(minor >> 4), (int)(minor & 0x0f));
	memcpy(&buf[32], rev, 4);
//
// The following code worked with the modified Apple CD-ROM drivers. Need to
// test with the original code to see if it works as well....
//	buf[4] = 42;	// Required
//
//	// Fill with blanks
//	memset(&buf[8], 0x20, buf[4] - 3);
//
//	// Vendor name
//	memcpy(&buf[8], BENDER_SIGNATURE, strlen(BENDER_SIGNATURE));
//
//	// Product name
//	memcpy(&buf[16], "CD-ROM CDU-8003A", 16);
//
//	// Revision (XM6 version number)
////	sprintf(rev, "1.9a",
//	////			(int)major, (int)(minor >> 4), (int)(minor & 0x0f));
//	memcpy(&buf[32], "1.9a", 4);
//
//	//strcpy(&buf[35],"A1.9a");
//	buf[36]=0x20;
//	memcpy(&buf[37],"1999/01/01",10);

	// Size of data that can be returned
	size = (buf[4] + 5);

	// Limit if the other buffer is small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	//  Success
	disk.code = DISK_NOERROR;
	return size;
}

//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::Read(BYTE *buf, DWORD block)
{
	int index;
	Filepath path;

	ASSERT(this);
	ASSERT(buf);

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Search for the track
	index = SearchTrack(block);

	// if invalid, out of range
	if (index < 0) {
		disk.code = DISK_INVALIDLBA;
		return 0;
	}
	ASSERT(track[index]);

	// If different from the current data track
	if (dataindex != index) {
		// Delete current disk cache (no need to save)
		delete disk.dcache;
		disk.dcache = NULL;

		// Reset the number of blocks
		disk.blocks = track[index]->GetBlocks();
		ASSERT(disk.blocks > 0);

		// Recreate the disk cache
		track[index]->GetPath(path);
		disk.dcache = new DiskCache(path, disk.size, disk.blocks);
		disk.dcache->SetRawMode(rawfile);

		// Reset data index
		dataindex = index;
	}

	// Base class
	ASSERT(dataindex >= 0);
	return Disk::Read(buf, block);
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::ReadToc(const DWORD *cdb, BYTE *buf)
{
	int last;
	int index;
	int length;
	int loop;
	int i;
	BOOL msf;
	DWORD lba;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x43);
	ASSERT(buf);

	// Check if ready
	if (!CheckReady()) {
		return 0;
	}

	// If ready, there is at least one track
	ASSERT(tracks > 0);
	ASSERT(track[0]);

	// Get allocation length, clear buffer
	length = cdb[7] << 8;
	length |= cdb[8];
	memset(buf, 0, length);

	// Get MSF Flag
	if (cdb[1] & 0x02) {
		msf = TRUE;
	} else {
		msf = FALSE;
	}

	// Get and check the last track number
	last = track[tracks - 1]->GetTrackNo();
	if ((int)cdb[6] > last) {
		// Except for AA
		if (cdb[6] != 0xaa) {
			disk.code = DISK_INVALIDCDB;
			return 0;
		}
	}

	// Check start index
	index = 0;
	if (cdb[6] != 0x00) {
		// Advance the track until the track numbers match
		while (track[index]) {
			if ((int)cdb[6] == track[index]->GetTrackNo()) {
				break;
			}
			index++;
		}

		// AA if not found or internal error
		if (!track[index]) {
			if (cdb[6] == 0xaa) {
				// Returns the final LBA+1 because it is AA
				buf[0] = 0x00;
				buf[1] = 0x0a;
				buf[2] = (BYTE)track[0]->GetTrackNo();
				buf[3] = (BYTE)last;
				buf[6] = 0xaa;
				lba = track[tracks - 1]->GetLast() + 1;
				if (msf) {
					LBAtoMSF(lba, &buf[8]);
				} else {
					buf[10] = (BYTE)(lba >> 8);
					buf[11] = (BYTE)lba;
				}
				return length;
			}

			// Otherwise, error
			disk.code = DISK_INVALIDCDB;
			return 0;
		}
	}

	// Number of track descriptors returned this time (number of loops)
	loop = last - track[index]->GetTrackNo() + 1;
	ASSERT(loop >= 1);

	// Create header
	buf[0] = (BYTE)(((loop << 3) + 2) >> 8);
	buf[1] = (BYTE)((loop << 3) + 2);
	buf[2] = (BYTE)track[0]->GetTrackNo();
	buf[3] = (BYTE)last;
	buf += 4;

	// Loop....
	for (i = 0; i < loop; i++) {
		// ADR and Control
		if (track[index]->IsAudio()) {
			// audio track
			buf[1] = 0x10;
		} else {
			// data track
			buf[1] = 0x14;
		}

		// track number
		buf[2] = (BYTE)track[index]->GetTrackNo();

		// track address
		if (msf) {
			LBAtoMSF(track[index]->GetFirst(), &buf[4]);
		} else {
			buf[6] = (BYTE)(track[index]->GetFirst() >> 8);
			buf[7] = (BYTE)(track[index]->GetFirst());
		}

		// Advance buffer pointer and index
		buf += 8;
		index++;
	}

	// Always return only the allocation length
	return length;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudio(const DWORD* /*cdb*/)
{
	ASSERT(this);

	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO MSF
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudioMSF(const DWORD* /*cdb*/)
{
	ASSERT(this);

	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO TRACK
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudioTrack(const DWORD* /*cdb*/)
{
	ASSERT(this);

	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	LBA→MSF Conversion
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::LBAtoMSF(DWORD lba, BYTE *msf) const
{
	DWORD m;
	DWORD s;
	DWORD f;

	ASSERT(this);

	// 75 and 75*60 get the remainder
	m = lba / (75 * 60);
	s = lba % (75 * 60);
	f = s % 75;
	s /= 75;

	// The base point is M=0, S=2, F=0
	s += 2;
	if (s >= 60) {
		s -= 60;
		m++;
	}

	// Store
	ASSERT(m < 0x100);
	ASSERT(s < 60);
	ASSERT(f < 75);
	msf[0] = 0x00;
	msf[1] = (BYTE)m;
	msf[2] = (BYTE)s;
	msf[3] = (BYTE)f;
}

//---------------------------------------------------------------------------
//
//	MSF→LBA Conversion
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSICD::MSFtoLBA(const BYTE *msf) const
{
	DWORD lba;

	ASSERT(this);
	ASSERT(msf[2] < 60);
	ASSERT(msf[3] < 75);

	// 1, 75, add up in multiples of 75*60
	lba = msf[1];
	lba *= 60;
	lba += msf[2];
	lba *= 75;
	lba += msf[3];

	// Since the base point is M=0, S=2, F=0, subtract 150
	lba -= 150;

	return lba;
}

//---------------------------------------------------------------------------
//
//	Clear Track
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::ClearTrack()
{
	int i;

	ASSERT(this);

	// delete the track object
	for (i = 0; i < TrackMax; i++) {
		if (track[i]) {
			delete track[i];
			track[i] = NULL;
		}
	}

	// Number of tracks is 0
	tracks = 0;

	// No settings for data and audio
	dataindex = -1;
	audioindex = -1;
}

//---------------------------------------------------------------------------
//
//	Track Search
//	* Returns -1 if not found
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::SearchTrack(DWORD lba) const
{
	int i;

	ASSERT(this);

	// Track loop
	for (i = 0; i < tracks; i++) {
		// Listen to the track
		ASSERT(track[i]);
		if (track[i]->IsValid(lba)) {
			return i;
		}
	}

	// Track wasn't found
	return -1;
}

//---------------------------------------------------------------------------
//
//	Next Frame
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::NextFrame()
{
	ASSERT(this);
	ASSERT((frame >= 0) && (frame < 75));

	// set the frame in the range 0-74
	frame = (frame + 1) % 75;

	// FALSE after one lap
	if (frame != 0) {
		return TRUE;
	} else {
		return FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Get CD-DA buffer
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::GetBuf(
	DWORD* /*buffer*/, int /*samples*/, DWORD /*rate*/)
{
	ASSERT(this);
}
