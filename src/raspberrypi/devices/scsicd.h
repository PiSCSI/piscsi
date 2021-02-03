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
	CDTrack(SCSICD *scsicd);
										// Constructor
	virtual ~CDTrack();
										// Destructor
	BOOL FASTCALL Init(int track, DWORD first, DWORD last);
										// Initialization

	// Properties
	void FASTCALL SetPath(BOOL cdda, const Filepath& path);
										// Set the path
	void FASTCALL GetPath(Filepath& path) const;
										// Get the path
	void FASTCALL AddIndex(int index, DWORD lba);
										// Add index
	DWORD FASTCALL GetFirst() const;
										// Get the start LBA
	DWORD FASTCALL GetLast() const;
										// Get the last LBA
	DWORD FASTCALL GetBlocks() const;
										// Get the number of blocks
	int FASTCALL GetTrackNo() const;
										// Get the track number
	BOOL FASTCALL IsValid(DWORD lba) const;
										// Is this a valid LBA?
	BOOL FASTCALL IsAudio() const;
										// Is this an audio track?

private:
	SCSICD *cdrom;
										// Parent device
	BOOL valid;
										// Valid track
	int track_no;
										// Track number
	DWORD first_lba;
										// First LBA
	DWORD last_lba;
										// Last LBA
	BOOL audio;
										// Audio track flag
	BOOL raw;
										// RAW data flag
	Filepath imgpath;
										// Image file path
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
	CDDABuf();
										// Constructor
	virtual ~CDDABuf();
										// Destructor
#if 0
	BOOL Init();
										// Initialization
	BOOL FASTCALL Load(const Filepath& path);
										// Load
	BOOL FASTCALL Save(const Filepath& path);
										// Save

	// API
	void FASTCALL Clear();
										// Clear the buffer
	BOOL FASTCALL Open(Filepath& path);
										// File specification
	BOOL FASTCALL GetBuf(DWORD *buffer, int frames);
										// Get the buffer
	BOOL FASTCALL IsValid();
										// Check if Valid
	BOOL FASTCALL ReadReq();
										// Read Request
	BOOL FASTCALL IsEnd() const;
										// Finish check

private:
	Filepath wavepath;
										// Wave path
	BOOL valid;
										// Open result (is it valid?)
	DWORD *buf;
										// Data buffer
	DWORD read;
										// Read pointer
	DWORD write;
										// Write pointer
	DWORD num;
										// Valid number of data
	DWORD rest;
										// Remaining file size
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
		TrackMax = 96					// Maximum number of tracks
	};

public:
	// Basic Functions
	SCSICD();
										// Constructor
	virtual ~SCSICD();
										// Destructor
	BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open
#ifndef	RASCSI
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
#endif	// RASCSI

	// commands
	int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command
	int FASTCALL Read(BYTE *buf, DWORD block);
										// READ command
	int FASTCALL ReadToc(const DWORD *cdb, BYTE *buf);
										// READ TOC command
	BOOL FASTCALL PlayAudio(const DWORD *cdb);
										// PLAY AUDIO command
	BOOL FASTCALL PlayAudioMSF(const DWORD *cdb);
										// PLAY AUDIO MSF command
	BOOL FASTCALL PlayAudioTrack(const DWORD *cdb);
										// PLAY AUDIO TRACK command

	// CD-DA
	BOOL FASTCALL NextFrame();
										// Frame notification
	void FASTCALL GetBuf(DWORD *buffer, int samples, DWORD rate);
										// Get CD-DA buffer

	// LBA-MSF変換
	void FASTCALL LBAtoMSF(DWORD lba, BYTE *msf) const;
										// LBA→MSF conversion
	DWORD FASTCALL MSFtoLBA(const BYTE *msf) const;
										// MSF→LBA conversion

private:
	// Open
	BOOL FASTCALL OpenCue(const Filepath& path);
										// Open(CUE)
	BOOL FASTCALL OpenIso(const Filepath& path);
										// Open(ISO)
	BOOL FASTCALL OpenPhysical(const Filepath& path);
										// Open(Physical)
	BOOL rawfile;
										// RAW flag

	// Track management
	void FASTCALL ClearTrack();
										// Clear the track
	int FASTCALL SearchTrack(DWORD lba) const;
										// Track search
	CDTrack* track[TrackMax];
										// Track opbject references
	int tracks;
										// Effective number of track objects
	int dataindex;
										// Current data track
	int audioindex;
										// Current audio track

	int frame;
										// Frame number

#if 0
	CDDABuf da_buf;
										// CD-DA buffer
	int da_num;
										// Number of CD-DA tracks
	int da_cur;
										// CD-DA current track
	int da_next;
										// CD-DA next track
	BOOL da_req;
										// CD-DA data request
#endif
};
