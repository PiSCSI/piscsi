//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
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

#include "shared/piscsi_exceptions.h"
#include "scsi_command_util.h"
#include "scsicd.h"
#include <array>
#include <fstream>

using namespace scsi_defs;
using namespace scsi_command_util;

SCSICD::SCSICD(int lun, scsi_defs::scsi_level level) : Disk(SCCD, lun, { 512, 2048 }), scsi_level(level)
{
	SetReadOnly(true);
	SetRemovable(true);
	SetLockable(true);
}

bool SCSICD::Init(const param_map& params)
{
	Disk::Init(params);

	AddCommand(scsi_command::eCmdReadToc, [this] { ReadToc(); });

	return true;
}

void SCSICD::Open()
{
	assert(!IsReady());

	// Initialization, track clear
	SetBlockCount(0);
	rawfile = false;
	ClearTrack();

	// Default sector size is 2048 bytes
	SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 2048);

	if (GetFilename()[0] == '\\') {
		OpenPhysical();
	} else {
		// Judge whether it is a CUE sheet or an ISO file
		array<char, 4> cue;
		ifstream in(GetFilename(), ios::binary);
		in.read(cue.data(), cue.size());
		if (!in.good()) {
			throw io_exception("Can't read header of CD-ROM file '" + GetFilename() + "'");
		}

		// If it starts with FILE consider it a CUE sheet
		if (!strncasecmp(cue.data(), "FILE", cue.size())) {
			throw io_exception("CUE CD-ROM files are not supported");
		} else {
			OpenIso();
		}
	}

	Disk::ValidateFile();

	SetUpCache(0, rawfile);

	SetReadOnly(true);
	SetProtectable(false);

	if (IsReady()) {
		SetAttn(true);
	}
}

void SCSICD::OpenIso()
{
	const off_t size = GetFileSize();
	if (size < 2048) {
		throw io_exception("ISO CD-ROM file size must be at least 2048 bytes");
	}

	// Validate header
	array<char, 16> header;
	ifstream in(GetFilename(), ios::binary);
	in.read(header.data(), header.size());
	if (!in.good()) {
		throw io_exception("Can't read header of ISO CD-ROM file");
	}

	// Check if it is in RAW format
	array<char, 12> sync = {};
	// 00,FFx10,00 is presumed to be RAW format
	fill_n(sync.begin() + 1, 10, 0xff);
	rawfile = false;

	if (memcmp(header.data(), sync.data(), sync.size()) == 0) {
		// Supports MODE1/2048 or MODE1/2352 only
		if (header[15] != 0x01) {
			// Different mode
			throw io_exception("Illegal raw ISO CD-ROM file header");
		}

		rawfile = true;
	}

	if (rawfile) {
		if (size % 2536) {
			LogWarn("Raw ISO CD-ROM file size is not a multiple of 2536 bytes but is "
					+ to_string(size) + " bytes");
		}

		SetBlockCount(static_cast<uint32_t>(size / 2352));
	} else {
		SetBlockCount(static_cast<uint32_t>(size >> GetSectorSizeShiftCount()));
	}

	CreateDataTrack();
}

// TODO This code is only executed if the filename starts with a `\`, but fails to open files starting with `\`.
void SCSICD::OpenPhysical()
{
	off_t size = GetFileSize();
	if (size < 2048) {
		throw io_exception("CD-ROM file size must be at least 2048 bytes");
	}

	// Effective size must be a multiple of 512
	size = (size / 512) * 512;

	// Set the number of blocks
	SetBlockCount(static_cast<uint32_t>(size >> GetSectorSizeShiftCount()));

	CreateDataTrack();
}

void SCSICD::CreateDataTrack()
{
	// Create only one data track
	assert(!tracks.size());
	auto track = make_unique<CDTrack>();
	track->Init(1, 0, static_cast<int>(GetBlockCount()) - 1);
	track->SetPath(false, GetFilename());
	tracks.push_back(std::move(track));
	dataindex = 0;
}

void SCSICD::ReadToc()
{
	GetController()->SetLength(ReadTocInternal(GetController()->GetCmd(), GetController()->GetBuffer()));

	EnterDataInPhase();
}

vector<uint8_t> SCSICD::InquiryInternal() const
{
	return HandleInquiry(device_type::cd_rom, scsi_level, true);
}

void SCSICD::SetUpModePages(map<int, vector<byte>>& pages, int page, bool changeable) const
{
	Disk::SetUpModePages(pages, page, changeable);

	if (page == 0x0d || page == 0x3f) {
		AddCDROMPage(pages, changeable);
	}

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

int SCSICD::Read(span<uint8_t> buf, uint64_t block)
{
	CheckReady();

	const int index = SearchTrack(static_cast<int>(block));
	if (index < 0) {
		throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
	}

	assert(tracks[index]);

	// If different from the current data track
	if (dataindex != index) {
		// Reset the number of blocks
		SetBlockCount(tracks[index]->GetBlocks());
		assert(GetBlockCount() > 0);

		// Re-assign disk cache (no need to save)
		ResizeCache(tracks[index]->GetPath(), rawfile);

		// Reset data index
		dataindex = index;
	}

	assert(dataindex >= 0);
	return Disk::Read(buf, block);
}

int SCSICD::ReadTocInternal(cdb_t cdb, vector<uint8_t>& buf)
{
	CheckReady();

	// If ready, there is at least one track
	assert(!tracks.empty());
	assert(tracks[0]);

	// Get allocation length, clear buffer
	const int length = GetInt16(cdb, 7);
	fill_n(buf.data(), length, 0);

	// Get MSF Flag
	const bool msf = cdb[1] & 0x02;

	// Get and check the last track number
	const int last = tracks[tracks.size() - 1]->GetTrackNo();
	// Except for AA
	if (cdb[6] > last && cdb[6] != 0xaa) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
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
			if (cdb[6] != 0xaa) {
				throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
			}

			// Returns the final LBA+1 because it is AA
			buf[0] = 0x00;
			buf[1] = 0x0a;
			buf[2] = (uint8_t)tracks[0]->GetTrackNo();
			buf[3] = (uint8_t)last;
			buf[6] = 0xaa;
			const uint32_t lba = tracks[tracks.size() - 1]->GetLast() + 1;
			if (msf) {
				LBAtoMSF(lba, &buf[8]);
			} else {
				SetInt16(buf, 10, lba);
			}

			return length;
		}
	}

	// Number of track descriptors returned this time (number of loops)
	const int loop = last - tracks[index]->GetTrackNo() + 1;
	assert(loop >= 1);

	// Create header
	SetInt16(buf, 0, (loop << 3) + 2);
	buf[2] = (uint8_t)tracks[0]->GetTrackNo();
	buf[3] = (uint8_t)last;

	int offset = 4;

	for (int i = 0; i < loop; i++) {
		// ADR and Control
		if (tracks[index]->IsAudio()) {
			// audio track
			buf[offset + 1] = 0x10;
		} else {
			// data track
			buf[offset + 1] = 0x14;
		}

		// track number
		buf[offset + 2] = (uint8_t)tracks[index]->GetTrackNo();

		// track address
		if (msf) {
			LBAtoMSF(tracks[index]->GetFirst(), &buf[offset + 4]);
		} else {
			SetInt16(buf, offset + 6, tracks[index]->GetFirst());
		}

		// Advance buffer pointer and index
		offset += 8;
		index++;
	}

	// Always return only the allocation length
	return length;
}

void SCSICD::LBAtoMSF(uint32_t lba, uint8_t *msf) const
{
	// 75 and 75*60 get the remainder
	uint32_t m = lba / (75 * 60);
	uint32_t s = lba % (75 * 60);
	const uint32_t f = s % 75;
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
	msf[1] = (uint8_t)m;
	msf[2] = (uint8_t)s;
	msf[3] = (uint8_t)f;
}

void SCSICD::ClearTrack()
{
	tracks.clear();

	// No settings for data and audio
	dataindex = -1;
	audioindex = -1;
}

int SCSICD::SearchTrack(uint32_t lba) const
{
	// Track loop
	for (size_t i = 0; i < tracks.size(); i++) {
		// Listen to the track
		assert(tracks[i]);
		if (tracks[i]->IsValid(lba)) {
			return static_cast<int>(i);
		}
	}

	// Track wasn't found
	return -1;
}

