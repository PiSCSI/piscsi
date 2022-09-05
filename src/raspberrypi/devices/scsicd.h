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

#include "os.h"
#include "disk.h"
#include "filepath.h"
#include "interfaces/scsi_mmc_commands.h"
#include "interfaces/scsi_primary_commands.h"

class SCSICD;

//===========================================================================
//
//	CD-ROM Track
//
//===========================================================================
class CDTrack
{
private:

	friend class SCSICD;

	explicit CDTrack(SCSICD *scsicd);
	virtual ~CDTrack() = default;

public:

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
	SCSICD *cdrom;								// Parent device
	bool valid;								// Valid track
	int track_no;								// Track number
	DWORD first_lba;							// First LBA
	DWORD last_lba;								// Last LBA
	bool audio;								// Audio track flag
	bool raw;								// RAW data flag
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
	enum {
		TrackMax = 96							// Maximum number of tracks
	};

	explicit SCSICD(const unordered_set<uint32_t>&);
	~SCSICD();

	bool Dispatch() override;

	void Open(const Filepath& path) override;

	// Commands
	vector<BYTE> InquiryInternal() const override;
	int Read(const DWORD *cdb, BYTE *buf, uint64_t block) override;
	int ReadToc(const DWORD *cdb, BYTE *buf);

protected:

	void AddModePages(map<int, vector<BYTE>>&, int, bool) const override;

private:
	using super = Disk;

	Dispatcher<SCSICD> dispatcher;

	void AddCDROMPage(map<int, vector<BYTE>>&, bool) const;
	void AddCDDAPage(map<int, vector<BYTE>>&, bool) const;

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
	CDTrack* track[TrackMax] = {};			// Track opbject references
	int tracks = 0;							// Effective number of track objects
	int dataindex = -1;						// Current data track
	int audioindex = -1;					// Current audio track
};
