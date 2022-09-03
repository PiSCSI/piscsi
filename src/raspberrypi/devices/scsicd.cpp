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
//	[ SCSI CD-ROM ]
//
//---------------------------------------------------------------------------

#include "scsicd.h"
#include "fileio.h"
#include "rascsi_exceptions.h"

using namespace scsi_defs;

//===========================================================================
//
//     CD Track
//
//===========================================================================

CDTrack::CDTrack(SCSICD *scsicd)
{
	ASSERT(scsicd);

	// Set parent CD-ROM device
	cdrom = scsicd;

	// Track defaults to disabled
	valid = false;

	// Initialize other data
	track_no = -1;
	first_lba = 0;
	last_lba = 0;
	audio = false;
	raw = false;
}

void CDTrack::Init(int track, DWORD first, DWORD last)
{
	ASSERT(!valid);
	ASSERT(track >= 1);
	ASSERT(first < last);

	// Set and enable track number
	track_no = track;
	valid = TRUE;

	// Remember LBA
	first_lba = first;
	last_lba = last;
}

void CDTrack::SetPath(bool cdda, const Filepath& path)
{
	ASSERT(valid);

	// CD-DA or data
	audio = cdda;

	// Remember the path
	imgpath = path;
}

void CDTrack::GetPath(Filepath& path) const
{
	ASSERT(valid);

	// Return the path (by reference)
	path = imgpath;
}

void CDTrack::AddIndex(int index, DWORD lba)
{
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
DWORD CDTrack::GetFirst() const
{
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	return first_lba;
}

//---------------------------------------------------------------------------
//
//	Get the end of LBA
//
//---------------------------------------------------------------------------
DWORD CDTrack::GetLast() const
{
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	return last_lba;
}

DWORD CDTrack::GetBlocks() const
{
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	// Calculate from start LBA and end LBA
	return (DWORD)(last_lba - first_lba + 1);
}

int CDTrack::GetTrackNo() const
{
	ASSERT(valid);
	ASSERT(track_no >= 1);

	return track_no;
}

//---------------------------------------------------------------------------
//
//	Is valid block
//
//---------------------------------------------------------------------------
bool CDTrack::IsValid(DWORD lba) const
{
	// FALSE if the track itself is invalid
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

//---------------------------------------------------------------------------
//
//	Is audio track
//
//---------------------------------------------------------------------------
bool CDTrack::IsAudio() const
{
	assert(valid);

	return audio;
}

//===========================================================================
//
//	SCSI CD-ROM
//
//===========================================================================

SCSICD::SCSICD(const unordered_set<uint32_t>& sector_sizes) : Disk("SCCD"), ScsiMmcCommands(), FileSupport()
{
	SetSectorSizes(sector_sizes);

	dispatcher.AddCommand(eCmdReadToc, "ReadToc", &SCSICD::ReadToc);
	dispatcher.AddCommand(eCmdGetEventStatusNotification, "GetEventStatusNotification", &SCSICD::GetEventStatusNotification);
}

SCSICD::~SCSICD()
{
	ClearTrack();
}

bool SCSICD::Dispatch()
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, ctrl->cmd[0]) ? true : super::Dispatch();
}

void SCSICD::Open(const Filepath& path)
{
	assert(!IsReady());

	// Initialization, track clear
	SetBlockCount(0);
	rawfile = false;
	ClearTrack();

	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::ReadOnly)) {
		throw file_not_found_exception("Can't open CD-ROM file");
	}

	// Default sector size is 2048 bytes
	SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 2048);

	// Close and transfer for physical CD access
	if (path.GetPath()[0] == _T('\\')) {
		// Close
		fio.Close();

		// Open physical CD
		OpenPhysical(path);
	} else {
		// Get file size
        off_t size = fio.GetFileSize();
		if (size <= 4) {
			fio.Close();
			throw io_exception("CD-ROM file size must be at least 4 bytes");
		}

		// Judge whether it is a CUE sheet or an ISO file
		TCHAR file[5];
		fio.Read(file, 4);
		file[4] = '\0';
		fio.Close();

		// If it starts with FILE, consider it as a CUE sheet
		if (!strncasecmp(file, _T("FILE"), 4)) {
			// Open as CUE
			OpenCue(path);
		} else {
			// Open as ISO
			OpenIso(path);
		}
	}

	// Successful opening
	ASSERT(GetBlockCount() > 0);

	super::Open(path);
	FileSupport::SetPath(path);

	// Set RAW flag
	assert(disk.dcache);
	disk.dcache->SetRawMode(rawfile);

	// Attention if ready
	if (IsReady()) {
		SetAttn(true);
	}
}

void SCSICD::OpenCue(const Filepath& /*path*/)
{
	throw io_exception("Opening CUE CD-ROM files is not supported");
}

void SCSICD::OpenIso(const Filepath& path)
{
	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::ReadOnly)) {
		throw io_exception("Can't open ISO CD-ROM file");
	}

	// Get file size
	off_t size = fio.GetFileSize();
	if (size < 0x800) {
		fio.Close();
		throw io_exception("ISO CD-ROM file size must be at least 2048 bytes");
	}

	// Read the first 12 bytes and close
	BYTE header[12];
	if (!fio.Read(header, sizeof(header))) {
		fio.Close();
		throw io_exception("Can't read header of ISO CD-ROM file");
	}

	// Check if it is RAW format
	BYTE sync[12];
	memset(sync, 0xff, sizeof(sync));
	sync[0] = 0x00;
	sync[11] = 0x00;
	rawfile = false;
	if (memcmp(header, sync, sizeof(sync)) == 0) {
		// 00,FFx10,00, so it is presumed to be RAW format
		if (!fio.Read(header, 4)) {
			fio.Close();
			throw io_exception("Can't read header of raw ISO CD-ROM file");
		}

		// Supports MODE1/2048 or MODE1/2352 only
		if (header[3] != 0x01) {
			// Different mode
			fio.Close();
			throw io_exception("Illegal raw ISO CD-ROM file header");
		}

		// Set to RAW file
		rawfile = true;
	}
	fio.Close();

	if (rawfile) {
		// Size must be a multiple of 2536
		if (size % 2536) {
			throw io_exception("Raw ISO CD-ROM file size must be a multiple of 2536 bytes but is "
					+ to_string(size) + " bytes");
		}

		// Set the number of blocks
		SetBlockCount((DWORD)(size / 0x930));
	} else {
		// Set the number of blocks
		SetBlockCount((DWORD)(size >> GetSectorSizeShiftCount()));
	}

	// Create only one data track
	assert(!track[0]);
	track[0] = new CDTrack(this);
	track[0]->Init(1, 0, GetBlockCount() - 1);
	track[0]->SetPath(false, path);
	tracks = 1;
	dataindex = 0;
}

void SCSICD::OpenPhysical(const Filepath& path)
{
	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::ReadOnly)) {
		throw io_exception("Can't open CD-ROM file");
	}

	// Get size
	off_t size = fio.GetFileSize();
	if (size < 0x800) {
		fio.Close();
		throw io_exception("CD-ROM file size must be at least 2048 bytes");
	}

	// Close
	fio.Close();

	// Effective size must be a multiple of 512
	size = (size / 512) * 512;

	// Set the number of blocks
	SetBlockCount((DWORD)(size >> GetSectorSizeShiftCount()));

	// Create only one data track
	ASSERT(!track[0]);
	track[0] = new CDTrack(this);
	track[0]->Init(1, 0, GetBlockCount() - 1);
	track[0]->SetPath(false, path);
	tracks = 1;
	dataindex = 0;
}

void SCSICD::ReadToc()
{
	ctrl->length = ReadToc(ctrl->cmd, ctrl->buffer);

	EnterDataInPhase();
}

vector<BYTE> SCSICD::InquiryInternal() const
{
	return HandleInquiry(device_type::CD_ROM, scsi_level::SCSI_2, true);

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
}

void SCSICD::AddModePages(map<int, vector<BYTE>>& pages, int page, bool changeable) const
{
	super::AddModePages(pages, page, changeable);

	// Page code 13
	if (page == 0x0d || page == 0x3f) {
		AddCDROMPage(pages, changeable);
	}

	// Page code 14
	if (page == 0x0e || page == 0x3f) {
		AddCDDAPage(pages, changeable);
	}
}

void SCSICD::AddCDROMPage(map<int, vector<BYTE>>& pages, bool changeable) const
{
	vector<BYTE> buf(8);

	// No changeable area
	if (!changeable) {
		// 2 seconds for inactive timer
		buf[3] = 0x05;

		// MSF multiples are 60 and 75 respectively
		buf[5] = 60;
		buf[7] = 75;
	}

	pages[13] = buf;
}

void SCSICD::AddCDDAPage(map<int, vector<BYTE>>& pages, bool) const
{
	vector<BYTE> buf(16);

	// Audio waits for operation completion and allows
	// PLAY across multiple tracks

	pages[14] = buf;
}

int SCSICD::Read(const DWORD *cdb, BYTE *buf, uint64_t block)
{
	assert(buf);

	CheckReady();

	// Search for the track
	int index = SearchTrack(block);

	// If invalid, out of range
	if (index < 0) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
	}

	assert(track[index]);

	// If different from the current data track
	if (dataindex != index) {
		// Delete current disk cache (no need to save)
		delete disk.dcache;
		disk.dcache = nullptr;

		// Reset the number of blocks
		SetBlockCount(track[index]->GetBlocks());
		ASSERT(GetBlockCount() > 0);

		// Recreate the disk cache
		Filepath path;
		track[index]->GetPath(path);
		disk.dcache = new DiskCache(path, GetSectorSizeShiftCount(), GetBlockCount());
		disk.dcache->SetRawMode(rawfile);

		// Reset data index
		dataindex = index;
	}

	// Base class
	assert(dataindex >= 0);
	return super::Read(cdb, buf, block);
}

int SCSICD::ReadToc(const DWORD *cdb, BYTE *buf)
{
	assert(cdb);
	assert(buf);

	CheckReady();

	// If ready, there is at least one track
	assert(tracks > 0);
	assert(track[0]);

	// Get allocation length, clear buffer
	int length = cdb[7] << 8;
	length |= cdb[8];
	memset(buf, 0, length);

	// Get MSF Flag
	bool msf = cdb[1] & 0x02;

	// Get and check the last track number
	int last = track[tracks - 1]->GetTrackNo();
	if ((int)cdb[6] > last) {
		// Except for AA
		if (cdb[6] != 0xaa) {
			throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
		}
	}

	// Check start index
	int index = 0;
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
				DWORD lba = track[tracks - 1]->GetLast() + 1;
				if (msf) {
					LBAtoMSF(lba, &buf[8]);
				} else {
					buf[10] = (BYTE)(lba >> 8);
					buf[11] = (BYTE)lba;
				}
				return length;
			}

			// Otherwise, error
			throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
		}
	}

	// Number of track descriptors returned this time (number of loops)
	int loop = last - track[index]->GetTrackNo() + 1;
	assert(loop >= 1);

	// Create header
	buf[0] = (BYTE)(((loop << 3) + 2) >> 8);
	buf[1] = (BYTE)((loop << 3) + 2);
	buf[2] = (BYTE)track[0]->GetTrackNo();
	buf[3] = (BYTE)last;
	buf += 4;

	// Loop....
	for (int i = 0; i < loop; i++) {
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

void SCSICD::GetEventStatusNotification()
{
	if (!(ctrl->cmd[1] & 0x01)) {
		// Asynchronous notification is optional and not supported by rascsi
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	LOGTRACE("Received request for event polling, which is currently not supported");
	throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
}

//---------------------------------------------------------------------------
//
//	LBA→MSF Conversion
//
//---------------------------------------------------------------------------
void SCSICD::LBAtoMSF(DWORD lba, BYTE *msf) const
{
	// 75 and 75*60 get the remainder
	DWORD m = lba / (75 * 60);
	DWORD s = lba % (75 * 60);
	DWORD f = s % 75;
	s /= 75;

	// The base point is M=0, S=2, F=0
	s += 2;
	if (s >= 60) {
		s -= 60;
		m++;
	}

	// Store
	assert(m < 0x100);
	assert(s < 60);
	assert(f < 75);
	msf[0] = 0x00;
	msf[1] = (BYTE)m;
	msf[2] = (BYTE)s;
	msf[3] = (BYTE)f;
}

void SCSICD::ClearTrack()
{
	// delete the track object
	for (int i = 0; i < TrackMax; i++) {
		delete track[i];
		track[i] = nullptr;
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
int SCSICD::SearchTrack(DWORD lba) const
{
	// Track loop
	for (int i = 0; i < tracks; i++) {
		// Listen to the track
		assert(track[i]);
		if (track[i]->IsValid(lba)) {
			return i;
		}
	}

	// Track wasn't found
	return -1;
}

