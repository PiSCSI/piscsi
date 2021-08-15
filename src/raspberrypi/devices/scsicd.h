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
//	[ SCSI Hard Disk for Apple Macintosh ]
//
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "disk.h"
#include "filepath.h"


//---------------------------------------------------------------------------
//
//	Class precedence definition
//
//---------------------------------------------------------------------------
class SCSICD;

//===========================================================================
//
//	CD-ROM Track
//
//===========================================================================
class CDTrack
{
public:
	// Basic Functions
	CDTrack(SCSICD *scsicd);						// Constructor
	virtual ~CDTrack();							// Destructor
	void Init(int track, DWORD first, DWORD last);			// Initialization

	// Properties
	void SetPath(BOOL cdda, const Filepath& path);			// Set the path
	void GetPath(Filepath& path) const;				// Get the path
	void AddIndex(int index, DWORD lba);				// Add index
	DWORD GetFirst() const;					// Get the start LBA
	DWORD GetLast() const;						// Get the last LBA
	DWORD GetBlocks() const;					// Get the number of blocks
	int GetTrackNo() const;					// Get the track number
	BOOL IsValid(DWORD lba) const;					// Is this a valid LBA?
	BOOL IsAudio() const;						// Is this an audio track?

private:
	SCSICD *cdrom;								// Parent device
	BOOL valid;								// Valid track
	int track_no;								// Track number
	DWORD first_lba;							// First LBA
	DWORD last_lba;								// Last LBA
	BOOL audio;								// Audio track flag
	BOOL raw;								// RAW data flag
	Filepath imgpath;							// Image file path
};

//===========================================================================
//
//	SCSI CD-ROM
//
//===========================================================================
class SCSICD : public Disk, public FileSupport
{
public:
	// Number of tracks
	enum {
		TrackMax = 96							// Maximum number of tracks
	};

public:
	// Basic Functions
	SCSICD();								// Constructor
	~SCSICD();								// Destructor
	void Open(const Filepath& path);		// Open

	// commands
	int Inquiry(const DWORD *cdb, BYTE *buf) override;	// INQUIRY command
	int Read(const DWORD *cdb, BYTE *buf, DWORD block) override;		// READ command
	int ReadToc(const DWORD *cdb, BYTE *buf);			// READ TOC command

	// CD-DA
	// TODO Never called
	bool NextFrame();						// Frame notification
	// TODO Never called
	void GetBuf(DWORD *buffer, int samples, DWORD rate);		// Get CD-DA buffer

	// LBA-MSF変換
	void LBAtoMSF(DWORD lba, BYTE *msf) const;			// LBA→MSF conversion
	DWORD MSFtoLBA(const BYTE *msf) const;				// MSF→LBA conversion

private:
	// Open
	void OpenCue(const Filepath& path);				// Open(CUE)
	void OpenIso(const Filepath& path);				// Open(ISO)
	void OpenPhysical(const Filepath& path);			// Open(Physical)
	BOOL rawfile;								// RAW flag

	// Track management
	void ClearTrack();						// Clear the track
	int SearchTrack(DWORD lba) const;				// Track search
	CDTrack* track[TrackMax];						// Track opbject references
	int tracks;								// Effective number of track objects
	int dataindex;								// Current data track
	int audioindex;								// Current audio track

	int frame;								// Frame number
};
