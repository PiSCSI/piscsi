//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License. 
//	See LICENSE file in the project root folder.
//
//	[ SCSI CD-ROM for Apple Macintosh ]
//
//---------------------------------------------------------------------------

#include "scsicd.h"
#include "fileio.h"
#include "exceptions.h"
#include <sstream>
#include "../rascsi.h"

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
	valid = false;

	// Initialize other data
	track_no = -1;
	first_lba = 0;
	last_lba = 0;
	audio = false;
	raw = false;
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

//---------------------------------------------------------------------------
//
//	Set Path
//
//---------------------------------------------------------------------------
void CDTrack::SetPath(bool cdda, const Filepath& path)
{
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
void CDTrack::GetPath(Filepath& path) const
{
	ASSERT(valid);

	// Return the path (by reference)
	path = imgpath;
}

//---------------------------------------------------------------------------
//
//	Add Index
//
//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
//
//	Get the number of blocks
//
//---------------------------------------------------------------------------
DWORD CDTrack::GetBlocks() const
{
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
	ASSERT(valid);

	return audio;
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
SCSICD::SCSICD() : Disk("SCCD"), ScsiMmcCommands(), FileSupport()
{
	// NOT in raw format
	rawfile = false;

	// Frame initialization
	frame = 0;

	// Track initialization
	for (int i = 0; i < TrackMax; i++) {
		track[i] = NULL;
	}
	tracks = 0;
	dataindex = -1;
	audioindex = -1;

	AddCommand(SCSIDEV::eCmdReadToc, "ReadToc", &SCSICD::ReadToc);
	AddCommand(SCSIDEV::eCmdGetEventStatusNotification, "GetEventStatusNotification", &SCSICD::GetEventStatusNotification);
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

	for (auto const& command : commands) {
		delete command.second;
	}
}

void SCSICD::AddCommand(SCSIDEV::scsi_command opcode, const char* name, void (SCSICD::*execute)(SASIDEV *))
{
	commands[opcode] = new command_t(name, execute);
}

bool SCSICD::Dispatch(SCSIDEV *controller)
{
	ctrl = controller->GetCtrl();

	if (commands.count(static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0]))) {
		command_t *command = commands[static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0])];

		LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, command->name, (unsigned int)ctrl->cmd[0]);

		(this->*command->execute)(controller);

		return true;
	}

	LOGTRACE("%s Calling base class for dispatching $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->cmd[0]);

	// The base class handles the less specific commands
	return Disk::Dispatch(controller);
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
void SCSICD::Open(const Filepath& path)
{
	off_t size;

	ASSERT(!IsReady());

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
	SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 2048, false);

	// Close and transfer for physical CD access
	if (path.GetPath()[0] == _T('\\')) {
		// Close
		fio.Close();

		// Open physical CD
		OpenPhysical(path);
	} else {
		// Get file size
        size = fio.GetFileSize();
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

	Disk::Open(path);
	FileSupport::SetPath(path);

	// Set RAW flag
	ASSERT(disk.dcache);
	disk.dcache->SetRawMode(rawfile);

	// Attention if ready
	if (IsReady()) {
		SetAttn(true);
	}
}

//---------------------------------------------------------------------------
//
//	Open (CUE)
//
//---------------------------------------------------------------------------
void SCSICD::OpenCue(const Filepath& /*path*/)
{
	throw io_exception("Opening CUE CD-ROM files is not supported");
}

//---------------------------------------------------------------------------
//
//	Open (ISO)
//
//---------------------------------------------------------------------------
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
			stringstream error;
			error << "Raw ISO CD-ROM file size must be a multiple of 2536 bytes but is " << size << " bytes";
			throw io_exception(error.str());
		}

		// Set the number of blocks
		SetBlockCount((DWORD)(size / 0x930));
	} else {
		// Set the number of blocks
		SetBlockCount((DWORD)(size >> GetSectorSizeShiftCount()));
	}

	// Create only one data track
	ASSERT(!track[0]);
	track[0] = new CDTrack(this);
	track[0]->Init(1, 0, GetBlockCount() - 1);
	track[0]->SetPath(false, path);
	tracks = 1;
	dataindex = 0;
}

//---------------------------------------------------------------------------
//
//	Open (Physical)
//
//---------------------------------------------------------------------------
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

void SCSICD::ReadToc(SASIDEV *controller)
{
	ctrl->length = ReadToc(ctrl->cmd, ctrl->buffer);
	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	controller->DataIn();
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int SCSICD::Inquiry(const DWORD *cdb, BYTE *buf)
{
	ASSERT(cdb);
	ASSERT(buf);

	// EVPD check
	if (cdb[1] & 0x01) {
		SetStatusCode(STATUS_INVALIDCDB);
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
	buf[1] = 0x80;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 36 - 5;	// Required

	// Fill with blanks
	memset(&buf[8], 0x20, buf[4] - 3);

	// Padded vendor, product, revision
	memcpy(&buf[8], GetPaddedName().c_str(), 28);

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
	int size = (buf[4] + 5);

	// Limit if the other buffer is small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	return size;
}

//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
int SCSICD::Read(const DWORD *cdb, BYTE *buf, uint64_t block)
{
	ASSERT(buf);

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Search for the track
	int index = SearchTrack(block);

	// if invalid, out of range
	if (index < 0) {
		SetStatusCode(STATUS_INVALIDLBA);
		return 0;
	}
	ASSERT(track[index]);

	// If different from the current data track
	if (dataindex != index) {
		// Delete current disk cache (no need to save)
		delete disk.dcache;
		disk.dcache = NULL;

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
	ASSERT(dataindex >= 0);
	return Disk::Read(cdb, buf, block);
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
int SCSICD::ReadToc(const DWORD *cdb, BYTE *buf)
{
	ASSERT(cdb);
	ASSERT(buf);

	// Check if ready
	if (!CheckReady()) {
		return 0;
	}

	// If ready, there is at least one track
	ASSERT(tracks > 0);
	ASSERT(track[0]);

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
			SetStatusCode(STATUS_INVALIDCDB);
			return 0;
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
			SetStatusCode(STATUS_INVALIDCDB);
			return 0;
		}
	}

	// Number of track descriptors returned this time (number of loops)
	int loop = last - track[index]->GetTrackNo() + 1;
	ASSERT(loop >= 1);

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

void SCSICD::GetEventStatusNotification(SASIDEV *controller)
{
	if (!(ctrl->cmd[1] & 0x01)) {
		// Asynchronous notification is optional and not supported by rascsi
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		return;
	}

	LOGTRACE("Received request for event polling, which is currently not supported");
	controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
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
//	Clear Track
//
//---------------------------------------------------------------------------
void SCSICD::ClearTrack()
{
	// delete the track object
	for (int i = 0; i < TrackMax; i++) {
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
int SCSICD::SearchTrack(DWORD lba) const
{
	// Track loop
	for (int i = 0; i < tracks; i++) {
		// Listen to the track
		ASSERT(track[i]);
		if (track[i]->IsValid(lba)) {
			return i;
		}
	}

	// Track wasn't found
	return -1;
}

