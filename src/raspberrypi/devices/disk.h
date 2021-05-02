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
#include "filepath.h"

//---------------------------------------------------------------------------
//
//	Error definition (sense code returned by REQUEST SENSE)
//
//	MSB		Reserved (0x00)
//			Sense Key
//			Additional Sense Code (ASC)
//	LSB		Additional Sense Code Qualifier(ASCQ)
//
//---------------------------------------------------------------------------
#define DISK_NOERROR		0x00000000	// NO ADDITIONAL SENSE INFO.
#define DISK_DEVRESET		0x00062900	// POWER ON OR RESET OCCURED
#define DISK_NOTREADY		0x00023a00	// MEDIUM NOT PRESENT
#define DISK_ATTENTION		0x00062800	// MEDIUM MAY HAVE CHANGED
#define DISK_PREVENT		0x00045302	// MEDIUM REMOVAL PREVENTED
#define DISK_READFAULT		0x00031100	// UNRECOVERED READ ERROR
#define DISK_WRITEFAULT		0x00030300	// PERIPHERAL DEVICE WRITE FAULT
#define DISK_WRITEPROTECT	0x00042700	// WRITE PROTECTED
#define DISK_MISCOMPARE		0x000e1d00	// MISCOMPARE DURING VERIFY
#define DISK_INVALIDCMD		0x00052000	// INVALID COMMAND OPERATION CODE
#define DISK_INVALIDLBA		0x00052100	// LOGICAL BLOCK ADDR. OUT OF RANGE
#define DISK_INVALIDCDB		0x00052400	// INVALID FIELD IN CDB
#define DISK_INVALIDLUN		0x00052500	// LOGICAL UNIT NOT SUPPORTED
#define DISK_INVALIDPRM		0x00052600	// INVALID FIELD IN PARAMETER LIST
#define DISK_INVALIDMSG		0x00054900	// INVALID MESSAGE ERROR
#define DISK_PARAMLEN		0x00051a00	// PARAMETERS LIST LENGTH ERROR
#define DISK_PARAMNOT		0x00052601	// PARAMETERS NOT SUPPORTED
#define DISK_PARAMVALUE		0x00052602	// PARAMETERS VALUE INVALID
#define DISK_PARAMSAVE		0x00053900	// SAVING PARAMETERS NOT SUPPORTED
#define DISK_NODEFECT		0x00010000	// DEFECT LIST NOT FOUND

#if 0
#define DISK_AUDIOPROGRESS	0x00??0011	// AUDIO PLAY IN PROGRESS
#define DISK_AUDIOPAUSED	0x00??0012	// AUDIO PLAY PAUSED
#define DISK_AUDIOSTOPPED	0x00??0014	// AUDIO PLAY STOPPED DUE TO ERROR
#define DISK_AUDIOCOMPLETE	0x00??0013	// AUDIO PLAY SUCCESSFULLY COMPLETED
#endif


#ifdef RASCSI
#define BENDER_SIGNATURE 	"RaSCSI"
// The following line was to mimic Apple's CDROM ID
// #define BENDER_SIGNATURE "SONY    "
#else
#define BENDER_SIGNATURE 	"XM6"
#endif

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
		off64_t imgoffset;						// Offset to actual data
	} disktrk_t;

public:
	// Basic Functions
	DiskTrack();								// Constructor
	virtual ~DiskTrack();							// Destructor
	void FASTCALL Init(int track, int size, int sectors, BOOL raw = FALSE, off64_t imgoff = 0);// Initialization
	BOOL FASTCALL Load(const Filepath& path);				// Load
	BOOL FASTCALL Save(const Filepath& path);				// Save

	// Read / Write
	BOOL FASTCALL Read(BYTE *buf, int sec) const;				// Sector Read
	BOOL FASTCALL Write(const BYTE *buf, int sec);				// Sector Write

	// Other
	int FASTCALL GetTrack() const		{ return dt.track; }		// Get track
	BOOL FASTCALL IsChanged() const		{ return dt.changed; }		// Changed flag check

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
	DiskCache(const Filepath& path, int size, int blocks,off64_t imgoff = 0);// Constructor
	virtual ~DiskCache();							// Destructor
	void FASTCALL SetRawMode(BOOL raw);					// CD-ROM raw mode setting

	// Access
	BOOL FASTCALL Save();							// Save and release all
	BOOL FASTCALL Read(BYTE *buf, int block);				// Sector Read
	BOOL FASTCALL Write(const BYTE *buf, int block);			// Sector Write
	BOOL FASTCALL GetCache(int index, int& track, DWORD& serial) const;	// Get cache information

private:
	// Internal Management
	void FASTCALL Clear();							// Clear all tracks
	DiskTrack* FASTCALL Assign(int track);					// Load track
	BOOL FASTCALL Load(int index, int track, DiskTrack *disktrk = NULL);	// Load track
	void FASTCALL Update();							// Update serial number

	// Internal data
	cache_t cache[CacheMax];						// Cache management
	DWORD serial;								// Last serial number
	Filepath sec_path;							// Path
	int sec_size;								// Sector size (8 or 9 or 11)
	int sec_blocks;								// Blocks per sector
	BOOL cd_raw;								// CD-ROM RAW mode
	off64_t imgoffset;							// Offset to actual data
};

//===========================================================================
//
//	Disk
//
//===========================================================================
class Disk
{
public:
	// Internal data structure
	typedef struct {
		DWORD id;							// Media ID
		BOOL ready;							// Valid Disk
		BOOL writep;							// Write protected
		BOOL readonly;							// Read only
		BOOL removable;							// Removable
		BOOL lock;							// Locked
		BOOL attn;							// Attention
		BOOL reset;							// Reset
		int size;							// Sector Size
		DWORD blocks;							// Total number of sectors
		DWORD lun;							// LUN
		DWORD code;							// Status code
		DiskCache *dcache;						// Disk cache
		off64_t imgoffset;						// Offset to actual data
	} disk_t;

public:
	// Basic Functions
	Disk();									// Constructor
	virtual ~Disk();							// Destructor
	virtual void FASTCALL Reset();						// Device Reset
	#ifndef RASCSI
	virtual BOOL FASTCALL Save(Fileio *fio, int ver);			// Save
	virtual BOOL FASTCALL Load(Fileio *fio, int ver);			// Load
	#endif	// RASCSI

	// ID
	DWORD FASTCALL GetID() const;						// Get media ID
	BOOL FASTCALL IsNULL() const;						// NULL check
	BOOL FASTCALL IsSASI() const;						// SASI Check
	BOOL FASTCALL IsSCSI() const;						// SASI Check

	// Media Operations
	virtual BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);	// Open
	void FASTCALL GetPath(Filepath& path) const;				// Get the path
	void FASTCALL Eject(BOOL force);					// Eject
	BOOL FASTCALL IsReady() const		{ return disk.ready; }		// Ready check
	void FASTCALL WriteP(BOOL flag);					// Set Write Protect flag
	BOOL FASTCALL IsWriteP() const		{ return disk.writep; }		// Get write protect flag
	BOOL FASTCALL IsReadOnly() const	{ return disk.readonly; }	// Get read only flag
	BOOL FASTCALL IsRemovable() const	{ return disk.removable; }	// Get is removable flag
	BOOL FASTCALL IsLocked() const		{ return disk.lock; }		// Get locked status
	BOOL FASTCALL IsAttn() const		{ return disk.attn; }		// Get attention flag
	BOOL FASTCALL Flush();							// Flush the cache
	void FASTCALL GetDisk(disk_t *buffer) const;				// Get the internal data struct

	// Properties
	void FASTCALL SetLUN(DWORD lun)		{ disk.lun = lun; }		// LUN set
	DWORD FASTCALL GetLUN()			{ return disk.lun; }		// LUN get

	// commands
	virtual int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);// INQUIRY command
	virtual int FASTCALL RequestSense(const DWORD *cdb, BYTE *buf);		// REQUEST SENSE command
	int FASTCALL SelectCheck(const DWORD *cdb);				// SELECT check
	int FASTCALL SelectCheck10(const DWORD *cdb);				// SELECT(10) check
	virtual BOOL FASTCALL ModeSelect(const DWORD *cdb, const BYTE *buf, int length);// MODE SELECT command
	virtual int FASTCALL ModeSense(const DWORD *cdb, BYTE *buf);		// MODE SENSE command
	virtual int FASTCALL ModeSense10(const DWORD *cdb, BYTE *buf);		// MODE SENSE(10) command
	int FASTCALL ReadDefectData10(const DWORD *cdb, BYTE *buf);		// READ DEFECT DATA(10) command
	virtual BOOL FASTCALL TestUnitReady(const DWORD *cdb);			// TEST UNIT READY command
	BOOL FASTCALL Rezero(const DWORD *cdb);					// REZERO command
	BOOL FASTCALL Format(const DWORD *cdb);					// FORMAT UNIT command
	BOOL FASTCALL Reassign(const DWORD *cdb);				// REASSIGN UNIT command
	virtual int FASTCALL Read(const DWORD *cdb, BYTE *buf, DWORD block);			// READ command
	virtual int FASTCALL WriteCheck(DWORD block);					// WRITE check
	virtual BOOL FASTCALL Write(const DWORD *cdb, const BYTE *buf, DWORD block);			// WRITE command
	BOOL FASTCALL Seek(const DWORD *cdb);					// SEEK command
	BOOL FASTCALL Assign(const DWORD *cdb);					// ASSIGN command
	BOOL FASTCALL Specify(const DWORD *cdb);				// SPECIFY command
	BOOL FASTCALL StartStop(const DWORD *cdb);				// START STOP UNIT command
	BOOL FASTCALL SendDiag(const DWORD *cdb);				// SEND DIAGNOSTIC command
	BOOL FASTCALL Removal(const DWORD *cdb);				// PREVENT/ALLOW MEDIUM REMOVAL command
	int FASTCALL ReadCapacity(const DWORD *cdb, BYTE *buf);			// READ CAPACITY command
	BOOL FASTCALL Verify(const DWORD *cdb);					// VERIFY command
	virtual int FASTCALL ReadToc(const DWORD *cdb, BYTE *buf);		// READ TOC command
	virtual BOOL FASTCALL PlayAudio(const DWORD *cdb);			// PLAY AUDIO command
	virtual BOOL FASTCALL PlayAudioMSF(const DWORD *cdb);			// PLAY AUDIO MSF command
	virtual BOOL FASTCALL PlayAudioTrack(const DWORD *cdb);			// PLAY AUDIO TRACK command
	void FASTCALL InvalidCmd();										// Unsupported command

	// Other
	BOOL FASTCALL IsCacheWB();								// Get cache writeback mode
	void FASTCALL SetCacheWB(BOOL enable);						// Set cache writeback mode

protected:
	// Internal processing
	virtual int FASTCALL AddError(BOOL change, BYTE *buf);			// Add error
	virtual int FASTCALL AddFormat(BOOL change, BYTE *buf);			// Add format
	virtual int FASTCALL AddDrive(BOOL change, BYTE *buf);			// Add drive
	int FASTCALL AddOpt(BOOL change, BYTE *buf);				// Add optical
	int FASTCALL AddCache(BOOL change, BYTE *buf);				// Add cache
	int FASTCALL AddCDROM(BOOL change, BYTE *buf);				// Add CD-ROM
	int FASTCALL AddCDDA(BOOL change, BYTE *buf);				// Add CD_DA
	virtual int FASTCALL AddVendor(int page, BOOL change, BYTE *buf);	// Add vendor special info
	BOOL FASTCALL CheckReady();						// Check if ready

	// Internal data

	char m_vendor_id[9];    // 8 characters + null
	char m_product_id[17];  // 16 characters + null
	char m_revision_lvl[5]; // 4 characters + null
	char m_serial_num[9];   // 8 characters + null

	disk_t disk;								// Internal disk data
	Filepath diskpath;							// File path (for GetPath)
	BOOL cache_wb;								// Cache mode
};
