//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	XM6i
//	Copyright (C) 2010-2015 isaki@NetBSD.org
//
//  	Imported sava's Anex86/T98Next image and MO format support patch.
//  	Comments translated to english by akuker.
//
//	[ Disk ]
//
//---------------------------------------------------------------------------

#pragma once

#include "xm6.h"
#include "log.h"
#include "scsi.h"
#include "block_device.h"
#include "file_support.h"
#include "filepath.h"
#include <string>

//===========================================================================
//
//	Disk Track
//
//===========================================================================
class DiskTrack
{
public:
	// Internal data definition
	typedef struct {
		int track;							// Track Number
		int size;							// Sector Size(8 or 9)
		int sectors;							// Number of sectors(<=0x100)
		DWORD length;							// Data buffer length
		BYTE *buffer;							// Data buffer
		BOOL init;							// Is it initilized?
		BOOL changed;							// Changed flag
		DWORD maplen;							// Changed map length
		BOOL *changemap;						// Changed map
		BOOL raw;							// RAW mode flag
		off_t imgoffset;						// Offset to actual data
	} disktrk_t;

public:
	// Basic Functions
	DiskTrack();								// Constructor
	virtual ~DiskTrack();							// Destructor
	void Init(int track, int size, int sectors, BOOL raw = FALSE, off_t imgoff = 0);// Initialization
	BOOL Load(const Filepath& path);				// Load
	BOOL Save(const Filepath& path);				// Save

	// Read / Write
	BOOL Read(BYTE *buf, int sec) const;				// Sector Read
	BOOL Write(const BYTE *buf, int sec);				// Sector Write

	// Other
	int GetTrack() const		{ return dt.track; }		// Get track
	BOOL IsChanged() const		{ return dt.changed; }		// Changed flag check

private:
	// Internal data
	disktrk_t dt;								// Internal data
};

//===========================================================================
//
//	Disk Cache
//
//===========================================================================
class DiskCache
{
public:
	// Internal data definition
	typedef struct {
		DiskTrack *disktrk;						// Disk Track
		DWORD serial;							// Serial
	} cache_t;

	// Number of caches
	enum {
		CacheMax = 16							// Number of tracks to cache
	};

public:
	// Basic Functions
	DiskCache(const Filepath& path, int size, int blocks,off_t imgoff = 0);// Constructor
	virtual ~DiskCache();							// Destructor
	void SetRawMode(BOOL raw);					// CD-ROM raw mode setting

	// Access
	BOOL Save();							// Save and release all
	BOOL Read(BYTE *buf, int block);				// Sector Read
	BOOL Write(const BYTE *buf, int block);			// Sector Write
	BOOL GetCache(int index, int& track, DWORD& serial) const;	// Get cache information

private:
	// Internal Management
	void Clear();							// Clear all tracks
	DiskTrack* Assign(int track);					// Load track
	BOOL Load(int index, int track, DiskTrack *disktrk = NULL);	// Load track
	void Update();							// Update serial number

	// Internal data
	cache_t cache[CacheMax];						// Cache management
	DWORD serial;								// Last serial number
	Filepath sec_path;							// Path
	int sec_size;								// Sector size (8 or 9 or 11)
	int sec_blocks;								// Blocks per sector
	BOOL cd_raw;								// CD-ROM RAW mode
	off_t imgoffset;							// Offset to actual data
};

//===========================================================================
//
//	Disk
//
//===========================================================================
class Disk : public BlockDevice
{
protected:
	// Internal data structure
	typedef struct {
		int size;							// Sector Size
		DWORD blocks;							// Total number of sectors
		DiskCache *dcache;						// Disk cache
		off_t imgoffset;						// Offset to actual data
	} disk_t;

public:
	// Basic Functions
	Disk(std::string);							// Constructor
	virtual ~Disk();							// Destructor

	// Media Operations
	virtual void Open(const Filepath& path);	// Open
	void GetPath(Filepath& path) const;				// Get the path
	bool Eject(bool) override;					// Eject
	bool Flush();							// Flush the cache

	// commands
	virtual bool TestUnitReady(const DWORD *cdb) override;	// TEST UNIT READY command
	virtual int Inquiry(const DWORD *cdb, BYTE *buf) override;	// INQUIRY command
	virtual int RequestSense(const DWORD *cdb, BYTE *buf) override;		// REQUEST SENSE command
	int SelectCheck(const DWORD *cdb);				// SELECT check
	int SelectCheck10(const DWORD *cdb);				// SELECT(10) check
	virtual bool ModeSelect(const DWORD *cdb, const BYTE *buf, int length) override;// MODE SELECT command
	virtual int ModeSense(const DWORD *cdb, BYTE *buf) override;		// MODE SENSE command
	virtual int ModeSense10(const DWORD *cdb, BYTE *buf) override;		// MODE SENSE(10) command
	int ReadDefectData10(const DWORD *cdb, BYTE *buf);		// READ DEFECT DATA(10) command
	bool Rezero(const DWORD *cdb);					// REZERO command
	bool Format(const DWORD *cdb) override;					// FORMAT UNIT command
	bool Reassign(const DWORD *cdb);				// REASSIGN UNIT command
	virtual int Read(const DWORD *cdb, BYTE *buf, DWORD block) override;			// READ command
	virtual int WriteCheck(DWORD block);					// WRITE check
	virtual bool Write(const DWORD *cdb, const BYTE *buf, DWORD block) override;			// WRITE command
	bool Seek(const DWORD *cdb);					// SEEK command
	bool Assign(const DWORD *cdb);					// ASSIGN command
	bool Specify(const DWORD *cdb);				// SPECIFY command
	bool StartStop(const DWORD *cdb);				// START STOP UNIT command
	bool SendDiag(const DWORD *cdb);				// SEND DIAGNOSTIC command
	bool Removal(const DWORD *cdb);				// PREVENT/ALLOW MEDIUM REMOVAL command
	int ReadCapacity10(const DWORD *cdb, BYTE *buf) override;			// READ CAPACITY(10) command
	int ReadCapacity16(const DWORD *cdb, BYTE *buf) override;			// READ CAPACITY(16) command
	int ReportLuns(const DWORD *cdb, BYTE *buf);				// REPORT LUNS command
	int GetSectorSize() const;
	void SetSectorSize(int);
	DWORD GetBlockCount() const;
	void SetBlockCount(DWORD);
	// TODO Currently not called
	bool Verify(const DWORD *cdb);					// VERIFY command
	virtual int ReadToc(const DWORD *cdb, BYTE *buf);		// READ TOC command
	virtual bool PlayAudio(const DWORD *cdb);			// PLAY AUDIO command
	virtual bool PlayAudioMSF(const DWORD *cdb);			// PLAY AUDIO MSF command
	virtual bool PlayAudioTrack(const DWORD *cdb);			// PLAY AUDIO TRACK command

protected:
	// Internal processing
	virtual int AddError(bool change, BYTE *buf);			// Add error
	virtual int AddFormat(bool change, BYTE *buf);			// Add format
	virtual int AddDrive(bool change, BYTE *buf);			// Add drive
	int AddOpt(bool change, BYTE *buf);				// Add optical
	int AddCache(bool change, BYTE *buf);				// Add cache
	int AddCDROM(bool change, BYTE *buf);				// Add CD-ROM
	int AddCDDA(bool, BYTE *buf);				// Add CD_DA
	virtual int AddVendor(int page, bool change, BYTE *buf);	// Add vendor special info
	BOOL CheckReady();						// Check if ready

	// Internal data
	disk_t disk;								// Internal disk data
	BOOL cache_wb;								// Cache mode
};
