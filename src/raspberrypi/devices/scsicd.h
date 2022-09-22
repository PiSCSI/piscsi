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
//	[ SCSI CD-ROM  ]
//
//---------------------------------------------------------------------------

#pragma once

#include "disk.h"
#include "filepath.h"
#include "interfaces/scsi_mmc_commands.h"
#include "interfaces/scsi_primary_commands.h"

//===========================================================================
//
//	CD-ROM Track
//
//===========================================================================
class CDTrack final
{

public:

	CDTrack() = default;
	~CDTrack() = default;
	CDTrack(CDTrack&) = delete;
	CDTrack& operator=(const CDTrack&) = delete;

	void Init(int track, DWORD first, DWORD last);

	// Properties
	void SetPath(bool cdda, const Filepath& path);			// Set the path
	void GetPath(Filepath& path) const;				// Get the path
	DWORD GetFirst() const;					// Get the start LBA
	DWORD GetLast() const;						// Get the last LBA
	DWORD GetBlocks() const;					// Get the number of blocks
	int GetTrackNo() const;					// Get the track number
	bool IsValid(DWORD lba) const;					// Is this a valid LBA?
	bool IsAudio() const;						// Is this an audio track?

private:
	bool valid = false;								// Valid track
	int track_no = -1;								// Track number
	DWORD first_lba = 0;							// First LBA
	DWORD last_lba = 0;								// Last LBA
	bool audio = false;								// Audio track flag
	Filepath imgpath;							// Image file path
};

//===========================================================================
//
//	SCSI CD-ROM
//
//===========================================================================
class SCSICD : public Disk, public ScsiMmcCommands, public FileSupport
{

public:

	explicit SCSICD(const unordered_set<uint32_t>&);
	~SCSICD() override;
	SCSICD(SCSICD&) = delete;
	SCSICD& operator=(const SCSICD&) = delete;

	bool Dispatch(scsi_command) override;

	void Open(const Filepath& path) override;

	// Commands
	vector<byte> InquiryInternal() const override;
	int Read(const vector<int>&, BYTE *, uint64_t) override;

protected:

	void AddModePages(map<int, vector<byte>>&, int, bool) const override;
	void AddVendorPage(map<int, vector<byte>>&, int, bool) const override;

private:

	using super = Disk;

	Dispatcher<SCSICD> dispatcher;

	int ReadTocInternal(const vector<int>&, BYTE *);

	void AddCDROMPage(map<int, vector<byte>>&, bool) const;
	void AddCDDAPage(map<int, vector<byte>>&, bool) const;

	// Open
	void OpenCue(const Filepath& path) const;
	void OpenIso(const Filepath& path);
	void OpenPhysical(const Filepath& path);

	void ReadToc() override;
	void GetEventStatusNotification() override;

	void LBAtoMSF(DWORD lba, BYTE *msf) const;			// LBA→MSF conversion

	bool rawfile = false;					// RAW flag

	// Track management
	void ClearTrack();						// Clear the track
	int SearchTrack(DWORD lba) const;		// Track search
	vector<unique_ptr<CDTrack>> tracks;		// Track opbject references
	int dataindex = -1;						// Current data track
	int audioindex = -1;					// Current audio track
};
