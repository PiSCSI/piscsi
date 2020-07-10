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
//  Imported sava's Anex86/T98Next image and MO format support patch.
//  Comments translated to english by akuker.
//
//	[ Disk ]
//
//---------------------------------------------------------------------------

#if !defined(disk_h)
#define disk_h

#include "log.h"
#include "scsi.h"

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
		int track;						// Track Number
		int size;						// Sector Size(8 or 9)
		int sectors;					// Number of sectors(<=0x100)
		DWORD length;					// Data buffer length
		BYTE *buffer;					// Data buffer
		BOOL init;						// Is it initilized?
		BOOL changed;					// Changed flag
		DWORD maplen;					// Changed map length
		BOOL *changemap;				// Changed map
		BOOL raw;						// RAW mode flag
		off64_t imgoffset;				// Offset to actual data
	} disktrk_t;

public:
	// Basic Functions
	DiskTrack();
										// Constructor
	virtual ~DiskTrack();
										// Destructor
	void FASTCALL Init(int track, int size, int sectors, BOOL raw = FALSE,
														 off64_t imgoff = 0);
										// Initialization
	BOOL FASTCALL Load(const Filepath& path);
										// Load
	BOOL FASTCALL Save(const Filepath& path);
										// Save

	// Read / Write
	BOOL FASTCALL Read(BYTE *buf, int sec) const;
										// Sector Read
	BOOL FASTCALL Write(const BYTE *buf, int sec);
										// Sector Write

	// Other
	int FASTCALL GetTrack() const		{ return dt.track; }
										// Get track
	BOOL FASTCALL IsChanged() const		{ return dt.changed; }
										// Changed flag check

private:
	// Internal data
	disktrk_t dt;
										// Internal data
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
		DiskTrack *disktrk;				// Disk Track
		DWORD serial;					// Serial
	} cache_t;

	// Number of caches
	enum {
		CacheMax = 16					// Number of tracks to cache
	};

public:
	// Basic Functions
	DiskCache(const Filepath& path, int size, int blocks,
														off64_t imgoff = 0);
										// Constructor
	virtual ~DiskCache();
										// Destructor
	void FASTCALL SetRawMode(BOOL raw);
										// CD-ROM raw mode setting

	// Access
	BOOL FASTCALL Save();
										// Save and release all
	BOOL FASTCALL Read(BYTE *buf, int block);
										// Sector Read
	BOOL FASTCALL Write(const BYTE *buf, int block);
										// Sector Write
	BOOL FASTCALL GetCache(int index, int& track, DWORD& serial) const;
										// Get cache information

private:
	// Internal Management
	void FASTCALL Clear();
										// Clear all tracks
	DiskTrack* FASTCALL Assign(int track);
										// Load track
	BOOL FASTCALL Load(int index, int track, DiskTrack *disktrk = NULL);
										// Load track
	void FASTCALL Update();
										// Update serial number

	// Internal data
	cache_t cache[CacheMax];
										// Cache management
	DWORD serial;
										// Last serial number
	Filepath sec_path;
										// Path
	int sec_size;
										// Sector size (8 or 9 or 11)
	int sec_blocks;
										// Blocks per sector
	BOOL cd_raw;
										// CD-ROM RAW mode
	off64_t imgoffset;
										// Offset to actual data
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
		DWORD id;						// Media ID
		BOOL ready;						// Valid Disk
		BOOL writep;					// Write protected
		BOOL readonly;					// Read only
		BOOL removable;					// Removable
		BOOL lock;						// Locked
		BOOL attn;						// Attention
		BOOL reset;						// Reset
		int size;						// Sector Size
		DWORD blocks;					// Total number of sectors
		DWORD lun;						// LUN
		DWORD code;						// Status code
		DiskCache *dcache;				// Disk cache
		off64_t imgoffset;				// Offset to actual data
	} disk_t;

public:
	// Basic Functions
	Disk();
										// Constructor
	virtual ~Disk();
										// Destructor
	virtual void FASTCALL Reset();
										// Device Reset
#ifndef RASCSI
	virtual BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	virtual BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
#endif	// RASCSI

	// ID
	DWORD FASTCALL GetID() const		{ return disk.id; }
										// Get media ID
	BOOL FASTCALL IsNULL() const;
										// NULL check
	BOOL FASTCALL IsSASI() const;
										// SASI Check

	// Media Operations
	virtual BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open
	void FASTCALL GetPath(Filepath& path) const;
										// Get the path
	void FASTCALL Eject(BOOL force);
										// Eject
	BOOL FASTCALL IsReady() const		{ return disk.ready; }
										// Ready check
	void FASTCALL WriteP(BOOL flag);
										// Set Write Protect flag
	BOOL FASTCALL IsWriteP() const		{ return disk.writep; }
										// Get write protect flag
	BOOL FASTCALL IsReadOnly() const	{ return disk.readonly; }
										// Get read only flag
	BOOL FASTCALL IsRemovable() const	{ return disk.removable; }
										// Get is removable flag
	BOOL FASTCALL IsLocked() const		{ return disk.lock; }
										// Get locked status
	BOOL FASTCALL IsAttn() const		{ return disk.attn; }
										// Get attention flag
	BOOL FASTCALL Flush();
										// Flush the cache
	void FASTCALL GetDisk(disk_t *buffer) const;
										// Get the internal data struct

	// Properties
	void FASTCALL SetLUN(DWORD lun)		{ disk.lun = lun; }
										// LUN set
	DWORD FASTCALL GetLUN()				{ return disk.lun; }
										// LUN get
	// commands
	virtual int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command
	virtual int FASTCALL RequestSense(const DWORD *cdb, BYTE *buf);
										// REQUEST SENSE command
	int FASTCALL SelectCheck(const DWORD *cdb);
										// SELECT check
	int FASTCALL SelectCheck10(const DWORD *cdb);
										// SELECT(10) check
	virtual BOOL FASTCALL ModeSelect(const DWORD *cdb, const BYTE *buf, int length);
										// MODE SELECT command
	virtual int FASTCALL ModeSense(const DWORD *cdb, BYTE *buf);
										// MODE SENSE command
	virtual int FASTCALL ModeSense10(const DWORD *cdb, BYTE *buf);
										// MODE SENSE(10) command
	int FASTCALL ReadDefectData10(const DWORD *cdb, BYTE *buf);
										// READ DEFECT DATA(10) command
	virtual BOOL FASTCALL TestUnitReady(const DWORD *cdb);
										// TEST UNIT READY command
	BOOL FASTCALL Rezero(const DWORD *cdb);
										// REZERO command
	BOOL FASTCALL Format(const DWORD *cdb);
										// FORMAT UNIT command
	BOOL FASTCALL Reassign(const DWORD *cdb);
										// REASSIGN UNIT command
	virtual int FASTCALL Read(BYTE *buf, DWORD block);
										// READ command
	int FASTCALL WriteCheck(DWORD block);
										// WRITE check
	BOOL FASTCALL Write(const BYTE *buf, DWORD block);
										// WRITE command
	BOOL FASTCALL Seek(const DWORD *cdb);
										// SEEK command
	BOOL FASTCALL Assign(const DWORD *cdb);
										// ASSIGN command
	BOOL FASTCALL Specify(const DWORD *cdb);
										// SPECIFY command
	BOOL FASTCALL StartStop(const DWORD *cdb);
										// START STOP UNIT command
	BOOL FASTCALL SendDiag(const DWORD *cdb);
										// SEND DIAGNOSTIC command
	BOOL FASTCALL Removal(const DWORD *cdb);
										// PREVENT/ALLOW MEDIUM REMOVAL command
	int FASTCALL ReadCapacity(const DWORD *cdb, BYTE *buf);
										// READ CAPACITY command
	BOOL FASTCALL Verify(const DWORD *cdb);
										// VERIFY command
	virtual int FASTCALL ReadToc(const DWORD *cdb, BYTE *buf);
										// READ TOC command
	virtual BOOL FASTCALL PlayAudio(const DWORD *cdb);
										// PLAY AUDIO command
	virtual BOOL FASTCALL PlayAudioMSF(const DWORD *cdb);
										// PLAY AUDIO MSF command
	virtual BOOL FASTCALL PlayAudioTrack(const DWORD *cdb);
										// PLAY AUDIO TRACK command
	void FASTCALL InvalidCmd()			{ disk.code = DISK_INVALIDCMD; }
										// Unsupported command

	// Other
	BOOL IsCacheWB() { return cache_wb; }
										// Get cache writeback mode
	void SetCacheWB(BOOL enable) { cache_wb = enable; }
										// Set cache writeback mode

protected:
	// Internal processing
	virtual int FASTCALL AddError(BOOL change, BYTE *buf);
										// Add error
	virtual int FASTCALL AddFormat(BOOL change, BYTE *buf);
										// Add format
	virtual int FASTCALL AddDrive(BOOL change, BYTE *buf);
										// Add drive
	int FASTCALL AddOpt(BOOL change, BYTE *buf);
										// Add optical
	int FASTCALL AddCache(BOOL change, BYTE *buf);
										// Add cache
	int FASTCALL AddCDROM(BOOL change, BYTE *buf);
										// Add CD-ROM
	int FASTCALL AddCDDA(BOOL change, BYTE *buf);
										// Add CD_DA
	virtual int FASTCALL AddVendor(int page, BOOL change, BYTE *buf);
										// Add vendor special info
	BOOL FASTCALL CheckReady();
										// Check if ready

	// Internal data
	disk_t disk;
										// Internal disk data
	Filepath diskpath;
										// File path (for GetPath)
	BOOL cache_wb;
										// Cache mode
};

//===========================================================================
//
//	SASI Hard Disk
//
//===========================================================================
class SASIHD : public Disk
{
public:
	// Basic Functions
	SASIHD();
										// Constructor
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open
	// commands
	int FASTCALL RequestSense(const DWORD *cdb, BYTE *buf);
										// REQUEST SENSE command
};

//===========================================================================
//
//	SCSI Hard Disk
//
//===========================================================================
class SCSIHD : public Disk
{
public:
	// Basic Functions
	SCSIHD();
										// Constructor
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open

	// commands
	int FASTCALL Inquiry(
		const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command
	BOOL FASTCALL ModeSelect(const DWORD *cdb, const BYTE *buf, int length);
										// MODE SELECT(6) command
};

//===========================================================================
//
//	SCSI hard disk (PC-9801-55 NEC genuine /Anex86/T98Next)
//
//===========================================================================
class SCSIHD_NEC : public SCSIHD
{
public:
	// Basic Functions
	SCSIHD_NEC();
										// Constructor

	BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open

	// commands
	int FASTCALL Inquiry(
		const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command

	// Internal processing
	int FASTCALL AddError(BOOL change, BYTE *buf);
										// Add error
	int FASTCALL AddFormat(BOOL change, BYTE *buf);
										// Add format
	int FASTCALL AddDrive(BOOL change, BYTE *buf);
										// Add drive

private:
	int cylinders;
										// Number of cylinders
	int heads;
										// Number of heads
	int sectors;
										// Number of sectors
	int sectorsize;
										// Sector size
	off64_t imgoffset;
										// Image offset
	off64_t imgsize;
										// Image size
};

//===========================================================================
//
//	SCSI Hard Disk(Genuine Apple Macintosh)
//
//===========================================================================
class SCSIHD_APPLE : public SCSIHD
{
public:
	// Basic Functions
	SCSIHD_APPLE();
										// Constructor
	// commands
	int FASTCALL Inquiry(
		const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command

	// Internal processing
	int FASTCALL AddVendor(int page, BOOL change, BYTE *buf);
										// Add vendor special page
};

//===========================================================================
//
//	SCSI magneto-optical disk
//
//===========================================================================
class SCSIMO : public Disk
{
public:
	// Basic Functions
	SCSIMO();
										// Constructor
	BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open
#ifndef RASCSI
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
#endif	// RASCSI

	// commands
	int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command
	BOOL FASTCALL ModeSelect(const DWORD *cdb, const BYTE *buf, int length);
										// MODE SELECT(6) command

	// Internal processing
	int FASTCALL AddVendor(int page, BOOL change, BYTE *buf);
										// Add vendor special page
};

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

//===========================================================================
//
//	SCSI Host Bridge
//
//===========================================================================
#if defined(RASCSI) && !defined(BAREMETAL)
class CTapDriver;
#endif	// RASCSI && !BAREMETAL
class CFileSys;
class SCSIBR : public Disk
{
public:
	// Basic Functions
	SCSIBR();
										// Constructor
	virtual ~SCSIBR();
										// Destructor

	// commands
	int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command
	BOOL FASTCALL TestUnitReady(const DWORD *cdb);
										// TEST UNIT READY command
	int FASTCALL GetMessage10(const DWORD *cdb, BYTE *buf);
										// GET MESSAGE10 command
	BOOL FASTCALL SendMessage10(const DWORD *cdb, BYTE *buf);
										// SEND MESSAGE10 command

private:
#if defined(RASCSI) && !defined(BAREMETAL)
	int FASTCALL GetMacAddr(BYTE *buf);
										// Get MAC address
	void FASTCALL SetMacAddr(BYTE *buf);
										// Set MAC address
	void FASTCALL ReceivePacket();
										// Receive a packet
	void FASTCALL GetPacketBuf(BYTE *buf);
										// Get a packet
	void FASTCALL SendPacket(BYTE *buf, int len);
										// Send a packet

	CTapDriver *tap;
										// TAP driver
	BOOL m_bTapEnable;
										// TAP valid flag
	BYTE mac_addr[6];
										// MAC Addres
	int packet_len;
										// Receive packet size
	BYTE packet_buf[0x1000];
										// Receive packet buffer
	BOOL packet_enable;
										// Received packet valid
#endif	// RASCSI && !BAREMETAL

	int FASTCALL ReadFsResult(BYTE *buf);
										// Read filesystem (result code)
	int FASTCALL ReadFsOut(BYTE *buf);
										// Read filesystem (return data)
	int FASTCALL ReadFsOpt(BYTE *buf);
										// Read file system (optional data)
	void FASTCALL WriteFs(int func, BYTE *buf);
										// File system write (execute)
	void FASTCALL WriteFsOpt(BYTE *buf, int len);
										// File system write (optional data)
	// Command handlers
	void FASTCALL FS_InitDevice(BYTE *buf);
										// $40 - boot
	void FASTCALL FS_CheckDir(BYTE *buf);
										// $41 - directory check
	void FASTCALL FS_MakeDir(BYTE *buf);
										// $42 - create directory
	void FASTCALL FS_RemoveDir(BYTE *buf);
										// $43 - delete directory
	void FASTCALL FS_Rename(BYTE *buf);
										// $44 - change filename
	void FASTCALL FS_Delete(BYTE *buf);
										// $45 - delete file
	void FASTCALL FS_Attribute(BYTE *buf);
										// $46 - get/set file attributes
	void FASTCALL FS_Files(BYTE *buf);
										// $47 - file search
	void FASTCALL FS_NFiles(BYTE *buf);
										// $48 - find next file
	void FASTCALL FS_Create(BYTE *buf);
										// $49 - create file
	void FASTCALL FS_Open(BYTE *buf);
										// $4A - open file
	void FASTCALL FS_Close(BYTE *buf);
										// $4B - close file
	void FASTCALL FS_Read(BYTE *buf);
										// $4C - read file
	void FASTCALL FS_Write(BYTE *buf);
										// $4D - write file
	void FASTCALL FS_Seek(BYTE *buf);
										// $4E - seek file
	void FASTCALL FS_TimeStamp(BYTE *buf);
										// $4F - get/set file time
	void FASTCALL FS_GetCapacity(BYTE *buf);
										// $50 - get capacity
	void FASTCALL FS_CtrlDrive(BYTE *buf);
										// $51 - drive status check/control
	void FASTCALL FS_GetDPB(BYTE *buf);
										// $52 - get DPB
	void FASTCALL FS_DiskRead(BYTE *buf);
										// $53 - read sector
	void FASTCALL FS_DiskWrite(BYTE *buf);
										// $54 - write sector
	void FASTCALL FS_Ioctrl(BYTE *buf);
										// $55 - IOCTRL
	void FASTCALL FS_Flush(BYTE *buf);
										// $56 - flush cache
	void FASTCALL FS_CheckMedia(BYTE *buf);
										// $57 - check media
	void FASTCALL FS_Lock(BYTE *buf);
										// $58 - get exclusive control

	CFileSys *fs;
										// File system accessor
	DWORD fsresult;
										// File system access result code
	BYTE fsout[0x800];
										// File system access result buffer
	DWORD fsoutlen;
										// File system access result buffer size
	BYTE fsopt[0x1000000];
										// File system access buffer
	DWORD fsoptlen;
										// File system access buffer size
};

//===========================================================================
//
//	SASI Controller
//
//===========================================================================
class SASIDEV
{
public:
	// Maximum number of logical units
	enum {
		UnitMax = 8
	};

#ifdef RASCSI
	// For timing adjustments
	enum {
		min_exec_time_sasi	= 100,			// SASI BOOT/FORMAT 30:NG 35:OK
		min_exec_time_scsi	= 50
	};
#endif	// RASCSI

	// Internal data definition
	typedef struct {
		// 全般
		BUS::phase_t phase;				// Transition phase
		int id;							// Controller ID (0-7)
		BUS *bus;						// Bus

		// commands
		DWORD cmd[10];					// Command data
		DWORD status;					// Status data
		DWORD message;					// Message data

#ifdef RASCSI
		// Run
		DWORD execstart;				// Execution start time
#endif	// RASCSI

		// Transfer
		BYTE *buffer;					// Transfer data buffer
		int bufsize;					// Transfer data buffer size
		DWORD blocks;					// Number of transfer block
		DWORD next;						// Next record
		DWORD offset;					// Transfer offset
		DWORD length;					// Transfer remaining length

		// Logical unit
		Disk *unit[UnitMax];
										// Logical Unit
	} ctrl_t;

public:
	// Basic Functions
#ifdef RASCSI
	SASIDEV();
#else
	SASIDEV(Device *dev);
#endif //RASCSI

										// Constructor
	virtual ~SASIDEV();
										// Destructor
	virtual void FASTCALL Reset();
										// Device Reset
#ifndef RASCSI
	virtual BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	virtual BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
#endif //RASCSI

	// External API
	virtual BUS::phase_t FASTCALL Process();
										// Run

	// Connect
	void FASTCALL Connect(int id, BUS *sbus);
										// Controller connection
	Disk* FASTCALL GetUnit(int no);
										// Get logical unit
	void FASTCALL SetUnit(int no, Disk *dev);
										// Logical unit setting
	BOOL FASTCALL HasUnit();
										// Has a valid logical unit

	// Other
	BUS::phase_t FASTCALL GetPhase() {return ctrl.phase;}
										// Get the phase
#ifdef DISK_LOG
	// Function to get the current phase as a String.
	void FASTCALL GetPhaseStr(char *str);
#endif

	int FASTCALL GetID() {return ctrl.id;}
										// Get the ID
	void FASTCALL GetCTRL(ctrl_t *buffer);
										// Get the internal information
	ctrl_t* FASTCALL GetWorkAddr() { return &ctrl; }
										// Get the internal information address
	virtual BOOL FASTCALL IsSASI() const {return TRUE;}
										// SASI Check
	virtual BOOL FASTCALL IsSCSI() const {return FALSE;}
										// SCSI check
	Disk* FASTCALL GetBusyUnit();
										// Get the busy unit

protected:
	// Phase processing
	virtual void FASTCALL BusFree();
										// Bus free phase
	virtual void FASTCALL Selection();
										// Selection phase
	virtual void FASTCALL Command();
										// Command phase
	virtual void FASTCALL Execute();
										// Execution phase
	void FASTCALL Status();
										// Status phase
	void FASTCALL MsgIn();
										// Message in phase
	void FASTCALL DataIn();
										// Data in phase
	void FASTCALL DataOut();
										// Data out phase
	virtual void FASTCALL Error();
										// Common error handling

	// commands
	void FASTCALL CmdTestUnitReady();
										// TEST UNIT READY command
	void FASTCALL CmdRezero();
										// REZERO UNIT command
	void FASTCALL CmdRequestSense();
										// REQUEST SENSE command
	void FASTCALL CmdFormat();
										// FORMAT command
	void FASTCALL CmdReassign();
										// REASSIGN BLOCKS command
	void FASTCALL CmdRead6();
										// READ(6) command
	void FASTCALL CmdWrite6();
										// WRITE(6) command
	void FASTCALL CmdSeek6();
										// SEEK(6) command
	void FASTCALL CmdAssign();
										// ASSIGN command
	void FASTCALL CmdSpecify();
										// SPECIFY command
	void FASTCALL CmdInvalid();
										// Unsupported command

	// データ転送
	virtual void FASTCALL Send();
										// Send data
#ifndef RASCSI
	virtual void FASTCALL SendNext();
										// Continue sending data
#endif	// RASCSI
	virtual void FASTCALL Receive();
										// Receive data
#ifndef RASCSI
	virtual void FASTCALL ReceiveNext();
										// Continue receiving data
#endif	// RASCSI
	BOOL FASTCALL XferIn(BYTE* buf);
										// Data transfer IN
	BOOL FASTCALL XferOut(BOOL cont);
										// Data transfer OUT

	// Special operations
	void FASTCALL FlushUnit();
										// Flush the logical unit
protected:
#ifndef RASCSI
	Device *host;
										// Host device
#endif // RASCSI

	ctrl_t ctrl;
										// Internal data
};

//===========================================================================
//
//	SCSI Device (Interits SASI device)
//
//===========================================================================
class SCSIDEV : public SASIDEV
{
public:
	// Internal data definition
	typedef struct {
		// Synchronous transfer
		BOOL syncenable;				// Synchronous transfer possible
		int syncperiod;					// Synchronous transfer period
		int syncoffset;					// Synchronous transfer offset
		int syncack;					// Number of synchronous transfer ACKs

		// ATN message
		BOOL atnmsg;
		int msc;
		BYTE msb[256];
	} scsi_t;

public:
	// Basic Functions
#ifdef RASCSI
	SCSIDEV();
#else
	SCSIDEV(Device *dev);
#endif // RASCSI
										// Constructor

	void FASTCALL Reset();
										// Device Reset

	// 外部API
	BUS::phase_t FASTCALL Process();
										// Run

	void FASTCALL SyncTransfer(BOOL enable) { scsi.syncenable = enable; }
										// Synchronouse transfer enable setting

	// Other
	BOOL FASTCALL IsSASI() const {return FALSE;}
										// SASI Check
	BOOL FASTCALL IsSCSI() const {return TRUE;}
										// SCSI check

private:
	// Phase
	void FASTCALL BusFree();
										// Bus free phase
	void FASTCALL Selection();
										// Selection phase
	void FASTCALL Execute();
										// Execution phase
	void FASTCALL MsgOut();
										// Message out phase
	void FASTCALL Error();
										// Common erorr handling

	// commands
	void FASTCALL CmdInquiry();
										// INQUIRY command
	void FASTCALL CmdModeSelect();
										// MODE SELECT command
	void FASTCALL CmdModeSense();
										// MODE SENSE command
	void FASTCALL CmdStartStop();
										// START STOP UNIT command
	void FASTCALL CmdSendDiag();
										// SEND DIAGNOSTIC command
	void FASTCALL CmdRemoval();
										// PREVENT/ALLOW MEDIUM REMOVAL command
	void FASTCALL CmdReadCapacity();
										// READ CAPACITY command
	void FASTCALL CmdRead10();
										// READ(10) command
	void FASTCALL CmdWrite10();
										// WRITE(10) command
	void FASTCALL CmdSeek10();
										// SEEK(10) command
	void FASTCALL CmdVerify();
										// VERIFY command
	void FASTCALL CmdSynchronizeCache();
										// SYNCHRONIZE CACHE  command
	void FASTCALL CmdReadDefectData10();
										// READ DEFECT DATA(10)  command
	void FASTCALL CmdReadToc();
										// READ TOC command
	void FASTCALL CmdPlayAudio10();
										// PLAY AUDIO(10) command
	void FASTCALL CmdPlayAudioMSF();
										// PLAY AUDIO MSF command
	void FASTCALL CmdPlayAudioTrack();
										// PLAY AUDIO TRACK INDEX command
	void FASTCALL CmdModeSelect10();
										// MODE SELECT(10) command
	void FASTCALL CmdModeSense10();
										// MODE SENSE(10) command
	void FASTCALL CmdGetMessage10();
										// GET MESSAGE(10) command
	void FASTCALL CmdSendMessage10();
										// SEND MESSAGE(10) command

	// データ転送
	void FASTCALL Send();
										// Send data
#ifndef RASCSI
	void FASTCALL SendNext();
										// Continue sending data
#endif	// RASCSI
	void FASTCALL Receive();
										// Receive data
#ifndef RASCSI
	void FASTCALL ReceiveNext();
										// Continue receiving data
#endif	// RASCSI
	BOOL FASTCALL XferMsg(DWORD msg);
										// Data transfer message

	scsi_t scsi;
										// Internal data
};



#endif	// disk_h
