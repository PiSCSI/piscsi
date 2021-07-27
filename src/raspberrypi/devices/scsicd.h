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
	bool Init(int track, DWORD first, DWORD last);			// Initialization

	// Properties
	void SetPath(bool cdda, const Filepath& path);			// Set the path
	void GetPath(Filepath& path) const;				// Get the path
	void AddIndex(int index, DWORD lba);				// Add index
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
//	CD-DA Buffer
//
//===========================================================================
class CDDABuf
{
public:
	// Basic Functions
	CDDABuf();								// Constructor
	virtual ~CDDABuf();							// Destructor
	#if 0
	bool Init();								// Initialization
	bool Load(const Filepath& path);				// Load
	bool Save(const Filepath& path);				// Save

	// API
	void Clear();							// Clear the buffer
	bool Open(Filepath& path);					// File specification
	bool GetBuf(DWORD *buffer, int frames);			// Get the buffer
	bool IsValid();						// Check if Valid
	bool ReadReq();						// Read Request
	bool IsEnd() const;						// Finish check

private:
	Filepath wavepath;							// Wave path
	bool valid;								// Open result (is it valid?)
	DWORD *buf;								// Data buffer
	DWORD read;								// Read pointer
	DWORD write;								// Write pointer
	DWORD num;								// Valid number of data
	DWORD rest;								// Remaining file size
#endif
};

//===========================================================================
//
//	SCSI CD-ROM
//
//===========================================================================
class SCSICD : public Disk
{
public:
	// Number of tracks
	enum {
		TrackMax = 96							// Maximum number of tracks
	};

public:
	// Basic Functions
	SCSICD();								// Constructor
	virtual ~SCSICD();							// Destructor
	bool Open(const Filepath& path, bool attn = true);		// Open

	// commands
	int Inquiry(const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);	// INQUIRY command
	int Read(const DWORD *cdb, BYTE *buf, DWORD block) override;		// READ command
	int ReadToc(const DWORD *cdb, BYTE *buf);			// READ TOC command
	bool PlayAudio(const DWORD *cdb);				// PLAY AUDIO command
	bool PlayAudioMSF(const DWORD *cdb);				// PLAY AUDIO MSF command
	bool PlayAudioTrack(const DWORD *cdb);				// PLAY AUDIO TRACK command

	// CD-DA
	bool NextFrame();						// Frame notification
	void GetBuf(DWORD *buffer, int samples, DWORD rate);		// Get CD-DA buffer

	// LBA-MSF変換
	void LBAtoMSF(DWORD lba, BYTE *msf) const;			// LBA→MSF conversion
	DWORD MSFtoLBA(const BYTE *msf) const;				// MSF→LBA conversion

private:
	// Open
	bool OpenCue(const Filepath& path);				// Open(CUE)
	bool OpenIso(const Filepath& path);				// Open(ISO)
	bool OpenPhysical(const Filepath& path);			// Open(Physical)
	bool rawfile;								// RAW flag

	// Track management
	void ClearTrack();						// Clear the track
	int SearchTrack(DWORD lba) const;				// Track search
	CDTrack* track[TrackMax];						// Track opbject references
	int tracks;								// Effective number of track objects
	int dataindex;								// Current data track
	int audioindex;								// Current audio track

	int frame;								// Frame number

	#if 0
	CDDABuf da_buf;								// CD-DA buffer
	int da_num;								// Number of CD-DA tracks
	int da_cur;								// CD-DA current track
	int da_next;								// CD-DA next track
	bool da_req;								// CD-DA data request
	#endif
};
