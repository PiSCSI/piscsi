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

#include "fileio.h"
#include "rascsi_exceptions.h"
#include "scsi_command_util.h"
#include "dispatcher.h"
#include "scsicd.h"
#include <array>

using namespace scsi_defs;
using namespace scsi_command_util;

SCSICD::SCSICD(const unordered_set<uint32_t>& sector_sizes) : Disk("SCCD")
{
	SetSectorSizes(sector_sizes);

	dispatcher.Add(scsi_command::eCmdReadToc, "ReadToc", &SCSICD::ReadToc);
	dispatcher.Add(scsi_command::eCmdGetEventStatusNotification, "GetEventStatusNotification", &SCSICD::GetEventStatusNotification);
}

SCSICD::~SCSICD()
{
	ClearTrack();
}

bool SCSICD::Dispatch(scsi_command cmd)
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, cmd) ? true : super::Dispatch(cmd);
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
	if (!fio.Open(path, Fileio::OpenMode::ReadOnly)) {
		throw file_not_found_exception("Can't open CD-ROM file");
	}

	// Default sector size is 2048 bytes
	SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 2048);

	// Close and transfer for physical CD access
	if (path.GetPath()[0] == '\\') {
		// Close
		fio.Close();

		// Open physical CD
		OpenPhysical(path);
	} else {
		// Get file size
		if (fio.GetFileSize() < 4) {
			fio.Close();
			throw io_exception("CD-ROM file size must be at least 4 bytes");
		}

		// Judge whether it is a CUE sheet or an ISO file
		array<TCHAR, 5> file;
		fio.Read((BYTE *)file.data(), 4);
		file[4] = '\0';
		fio.Close();

		// If it starts with FILE, consider it as a CUE sheet
		if (!strcasecmp(file.data(), "FILE")) {
			// Open as CUE
			OpenCue(path);
		} else {
			// Open as ISO
			OpenIso(path);
		}
	}

	// Successful opening
	assert(GetBlockCount() > 0);

	super::Open(path);
	FileSupport::SetPath(path);

	// Set RAW flag
	disk.dcache->SetRawMode(rawfile);

	// Attention if ready
	if (IsReady()) {
		SetAttn(true);
	}
}

void SCSICD::OpenCue(const Filepath& /*path*/) const
{
	throw io_exception("Opening CUE CD-ROM files is not supported");
}

void SCSICD::OpenIso(const Filepath& path)
{
	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::OpenMode::ReadOnly)) {
		throw io_exception("Can't open ISO CD-ROM file");
	}

	// Get file size
	off_t size = fio.GetFileSize();
	if (size < 0x800) {
		fio.Close();
		throw io_exception("ISO CD-ROM file size must be at least 2048 bytes");
	}

	// Read the first 12 bytes and close
	array<BYTE, 12> header;
	if (!fio.Read(header.data(), header.size())) {
		fio.Close();
		throw io_exception("Can't read header of ISO CD-ROM file");
	}

	// Check if it is RAW format
	array<BYTE, 12> sync;
	memset(sync.data(), 0xff, sync.size());
	sync[0] = 0x00;
	sync[11] = 0x00;
	rawfile = false;
	if (memcmp(header.data(), sync.data(), sync.size()) == 0) {
		// 00,FFx10,00, so it is presumed to be RAW format
		if (!fio.Read(header.data(), 4)) {
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
	assert(!tracks.size());
	auto track = make_unique<CDTrack>();
	track->Init(1, 0, (int)GetBlockCount() - 1);
	track->SetPath(false, path);
	tracks.push_back(move(track));
	dataindex = 0;
}

void SCSICD::OpenPhysical(const Filepath& path)
{
	// Open as read-only
	Fileio fio;
	if (!fio.Open(path, Fileio::OpenMode::ReadOnly)) {
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
	assert(!tracks.size());
	auto track = make_unique<CDTrack>();
	track->Init(1, 0, (int)GetBlockCount() - 1);
	track->SetPath(false, path);
	tracks.push_back(move(track));
	dataindex = 0;
}

void SCSICD::ReadToc()
{
	ctrl->length = ReadTocInternal(ctrl->cmd, ctrl->buffer);

	EnterDataInPhase();
}

vector<byte> SCSICD::InquiryInternal() const
{
	return HandleInquiry(device_type::CD_ROM, scsi_level::SCSI_2, true);
}

void SCSICD::SetUpModePages(map<int, vector<byte>>& pages, int page, bool changeable) const
{
	super::SetUpModePages(pages, page, changeable);

	// Page code 13
	if (page == 0x0d || page == 0x3f) {
		AddCDROMPage(pages, changeable);
	}

	// Page code 14
	if (page == 0x0e || page == 0x3f) {
		AddCDDAPage(pages, changeable);
	}
}

void SCSICD::AddCDROMPage(map<int, vector<byte>>& pages, bool changeable) const
{
	vector<byte> buf(8);

	// No changeable area
	if (!changeable) {
		// 2 seconds for inactive timer
		buf[3] = (byte)0x05;

		// MSF multiples are 60 and 75 respectively
		buf[5] = (byte)60;
		buf[7] = (byte)75;
	}

	pages[13] = buf;
}

void SCSICD::AddCDDAPage(map<int, vector<byte>>& pages, bool) const
{
	vector<byte> buf(16);

	// Audio waits for operation completion and allows
	// PLAY across multiple tracks

	pages[14] = buf;
}

void SCSICD::AddVendorPage(map<int, vector<byte>>& pages, int page, bool changeable) const
{
	// Page code 48
	if (page == 0x30 || page == 0x3f) {
		AddAppleVendorModePage(pages, changeable);
	}
}


int SCSICD::Read(const vector<int>& cdb, BYTE *buf, uint64_t block)
{
	assert(buf);

	CheckReady();

	// Search for the track
	int index = SearchTrack((int)block);

	// If invalid, out of range
	if (index < 0) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
	}

	assert(tracks[index]);

	// If different from the current data track
	if (dataindex != index) {
		// Delete current disk cache (no need to save)
		delete disk.dcache;
		disk.dcache = nullptr;

		// Reset the number of blocks
		SetBlockCount(tracks[index]->GetBlocks());
		assert(GetBlockCount() > 0);

		// Recreate the disk cache
		Filepath path;
		tracks[index]->GetPath(path);
		disk.dcache = new DiskCache(path, GetSectorSizeShiftCount(), (uint32_t)GetBlockCount());
		disk.dcache->SetRawMode(rawfile);

		// Reset data index
		dataindex = index;
	}

	// Base class
	assert(dataindex >= 0);
	return super::Read(cdb, buf, block);
}

int SCSICD::ReadTocInternal(const vector<int>& cdb, BYTE *buf)
{
	CheckReady();

	// If ready, there is at least one track
	assert(tracks.size() > 0);
	assert(tracks[0]);

	// Get allocation length, clear buffer
	int length = cdb[7] << 8;
	length |= cdb[8];
	memset(buf, 0, length);

	// Get MSF Flag
	bool msf = cdb[1] & 0x02;

	// Get and check the last track number
	int last = tracks[tracks.size() - 1]->GetTrackNo();
	// Except for AA
	if (cdb[6] > last && cdb[6] != 0xaa) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Check start index
	int index = 0;
	if (cdb[6] != 0x00) {
		// Advance the track until the track numbers match
		while (tracks[index]) {
			if (cdb[6] == tracks[index]->GetTrackNo()) {
				break;
			}
			index++;
		}

		// AA if not found or internal error
		if (!tracks[index]) {
			if (cdb[6] == 0xaa) {
				// Returns the final LBA+1 because it is AA
				buf[0] = 0x00;
				buf[1] = 0x0a;
				buf[2] = (BYTE)tracks[0]->GetTrackNo();
				buf[3] = (BYTE)last;
				buf[6] = 0xaa;
				uint32_t lba = tracks[tracks.size() - 1]->GetLast() + 1;
				if (msf) {
					LBAtoMSF(lba, &buf[8]);
				} else {
					SetInt16(&buf[10], lba);
				}
				return length;
			}

			// Otherwise, error
			throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
		}
	}

	// Number of track descriptors returned this time (number of loops)
	int loop = last - tracks[index]->GetTrackNo() + 1;
	assert(loop >= 1);

	// Create header
	SetInt16(&buf[0], (loop << 3) + 2);
	buf[2] = (BYTE)tracks[0]->GetTrackNo();
	buf[3] = (BYTE)last;
	buf += 4;

	// Loop....
	for (int i = 0; i < loop; i++) {
		// ADR and Control
		if (tracks[index]->IsAudio()) {
			// audio track
			buf[1] = 0x10;
		} else {
			// data track
			buf[1] = 0x14;
		}

		// track number
		buf[2] = (BYTE)tracks[index]->GetTrackNo();

		// track address
		if (msf) {
			LBAtoMSF(tracks[index]->GetFirst(), &buf[4]);
		} else {
			SetInt16(&buf[6], tracks[index]->GetFirst());
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

	LOGTRACE("Received request for event polling, which is currently not supported")
	throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
}

//---------------------------------------------------------------------------
//
//	LBA→MSF Conversion
//
//---------------------------------------------------------------------------
void SCSICD::LBAtoMSF(uint32_t lba, BYTE *msf) const
{
	// 75 and 75*60 get the remainder
	uint32_t m = lba / (75 * 60);
	uint32_t s = lba % (75 * 60);
	uint32_t f = s % 75;
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
	tracks.clear();

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
	for (size_t i = 0; i < tracks.size(); i++) {
		// Listen to the track
		assert(tracks[i]);
		if (tracks[i]->IsValid(lba)) {
			return (int)i;
		}
	}

	// Track wasn't found
	return -1;
}

