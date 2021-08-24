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
//	SCSI CD-ROM
//
//===========================================================================
class SCSICD : public Disk, public FileSupport
{
private:
	typedef struct _command_t {
		const char* name;
		void (SCSICD::*execute)(SASIDEV *);

		_command_t(const char* _name, void (SCSICD::*_execute)(SASIDEV *)) : name(_name), execute(_execute) { };
	} command_t;
	std::map<SCSIDEV::scsi_command, command_t*> commands;

	SASIDEV::ctrl_t *ctrl;

	void AddCommand(SCSIDEV::scsi_command, const char*, void (SCSICD::*)(SASIDEV *));

public:
	// Number of tracks
	enum {
		TrackMax = 96							// Maximum number of tracks
	};

public:
	// Basic Functions
	SCSICD();								// Constructor
	~SCSICD();								// Destructor
	void Open(const Filepath& path) override;		// Open

	// commands
	int Inquiry(const DWORD *cdb, BYTE *buf) override;	// INQUIRY command
	int Read(const DWORD *cdb, BYTE *buf, uint64_t block) override;		// READ command
	int ReadToc(const DWORD *cdb, BYTE *buf);			// READ TOC command

	bool Dispatch(SCSIDEV *);

private:
	// Open
	void OpenCue(const Filepath& path);				// Open(CUE)
	void OpenIso(const Filepath& path);				// Open(ISO)
	void OpenPhysical(const Filepath& path);			// Open(Physical)

	void ReadToc(SASIDEV *);
	void GetEventStatusNotification(SASIDEV *);

	// LBA-MSF変換
	void LBAtoMSF(DWORD lba, BYTE *msf) const;			// LBA→MSF conversion

	bool rawfile;								// RAW flag

	// Track management
	void ClearTrack();						// Clear the track
	int SearchTrack(DWORD lba) const;				// Track search
	CDTrack* track[TrackMax];						// Track opbject references
	int tracks;								// Effective number of track objects
	int dataindex;								// Current data track
	int audioindex;								// Current audio track

	int frame;								// Frame number
};
