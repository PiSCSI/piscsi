//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI (*^..^*)
//  for Raspberry Pi
//
//  Powered by XM6 TypeG Technology.
//  Copyright (C) 2016-2017 GIMONS
//	[ Host Filesystem Bridge Driver ]
//
//---------------------------------------------------------------------------

#ifndef bridge_h
#define bridge_h

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef int BOOL;

#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE 1
#endif

#define FASTCALL
#define ASSERT(x)

//---------------------------------------------------------------------------
//
//	Constant definitions
//
//---------------------------------------------------------------------------
#define FILEPATH_MAX		_MAX_PATH
#define WINDRV_CLIPSIZE_MAX	0xC00000

//---------------------------------------------------------------------------
//
//	Status code definitions
//
//---------------------------------------------------------------------------
#define FS_INVALIDFUNC		0xFFFFFFFF	// Executed an invalid function
#define FS_FILENOTFND		0xFFFFFFFE	// The selected file can not be found
#define FS_DIRNOTFND		0xFFFFFFFD	// The selected directory can not be found
#define FS_OVEROPENED		0xFFFFFFFC	// There are too many files open
#define FS_CANTACCESS		0xFFFFFFFB	// Can not access the direcory or volume
#define FS_NOTOPENED		0xFFFFFFFA	// The selected handle is not opened
#define FS_INVALIDMEM		0xFFFFFFF9	// Memory management has been destroyed
#define FS_OUTOFMEM			0xFFFFFFF8	// Insufficient memory for execution
#define FS_INVALIDPTR		0xFFFFFFF7	// Selected an invalid memory management pointer
#define FS_INVALIDENV		0xFFFFFFF6	// Selected an invalid environment
#define FS_ILLEGALFMT		0xFFFFFFF5	// The exeucted file is in an invalid format
#define FS_ILLEGALMOD		0xFFFFFFF4	// Invalid open access mode
#define FS_INVALIDPATH		0xFFFFFFF3	// Mistake in selected file name
#define FS_INVALIDPRM		0xFFFFFFF2	// Called with an invalid parameter
#define FS_INVALIDDRV		0xFFFFFFF1	// Mistake in selected drive
#define FS_DELCURDIR		0xFFFFFFF0	// Unable to delete the current directory
#define FS_NOTIOCTRL		0xFFFFFFEF	// Unable to use IOCTRL with the device
#define FS_LASTFILE			0xFFFFFFEE	// Can not find any more files
#define FS_CANTWRITE		0xFFFFFFED	// Selected file can not be written
#define FS_DIRALREADY		0xFFFFFFEC	// Selected directory is already registered
#define FS_CANTDELETE		0xFFFFFFEB	// Can not delete because of a file
#define FS_CANTRENAME		0xFFFFFFEA	// Can not rename because of a file
#define FS_DISKFULL			0xFFFFFFE9	// Can not create a file because the disk is full
#define FS_DIRFULL			0xFFFFFFE8	// Can not create a file because the directory is full
#define FS_CANTSEEK			0xFFFFFFE7	// Can not seek in the selected location
#define FS_SUPERVISOR		0xFFFFFFE6	// Selected supervisor in supervisor mode
#define FS_THREADNAME		0xFFFFFFE5	// A thread with this name already exists
#define FS_BUFWRITE			0xFFFFFFE4	// Writing to inter-process communication buffers is disallowed
#define FS_BACKGROUND		0xFFFFFFE3	// Unable to start a background process
#define FS_OUTOFLOCK		0xFFFFFFE0	// Insufficient lock space
#define FS_LOCKED			0xFFFFFFDF	// Can not access because it is locked
#define FS_DRIVEOPENED		0xFFFFFFDE	// Selected drive has an open handler
#define FS_LINKOVER			0xFFFFFFDD	// The symbolic link is nested over 16 times
#define FS_FILEEXIST		0xFFFFFFB0	// The file exists

#define FS_FATAL_MEDIAOFFLINE	0xFFFFFFA3	// No media inserted
#define FS_FATAL_WRITEPROTECT	0xFFFFFFA2	// Write protected
#define FS_FATAL_INVALIDCOMMAND	0xFFFFFFA1	// Invalid command number
#define FS_FATAL_INVALIDUNIT	0xFFFFFFA0	// Invalid unit number

#define HUMAN68K_PATH_MAX	96		// Longest path allowed in Human68k

//===========================================================================
//
/// Human68k name space
//
//===========================================================================
/// File attribute bit
enum attribute_t {
	AT_READONLY = 0x01,		// Read only attribute
	AT_HIDDEN = 0x02,		// Hidden attribute
	AT_SYSTEM = 0x04,		// System attribute
	AT_VOLUME = 0x08,		// Volume label attribute
	AT_DIRECTORY = 0x10,	// Directory attribute
	AT_ARCHIVE = 0x20,		// Archive attribute
	AT_ALL = 0xFF,			// All attribute bits are 1
};

/// File open modes
enum open_t {
	OP_READ = 0,			// Read
	OP_WRITE = 1,			// Write
	OP_FULL = 2,			// Read/Write
	OP_MASK = 0x0F,			// Decision mask
	OP_SHARE_NONE = 0x10,	// Sharing forbidden
	OP_SHARE_READ = 0x20,	// Read sharing
	OP_SHARE_WRITE = 0x30,	// Write sharing
	OP_SHARE_FULL = 0x40,	// Read/Write sharing
	OP_SHARE_MASK = 0x70,	// Sharing decision mask
	OP_SPECIAL = 0x100,		// Dictionary access
};

/// Seek types
enum seek_t {
	SK_BEGIN = 0,			// From the beginning of a file
	SK_CURRENT = 1,			// From the current location
	SK_END = 2,				// From the end of the file
};

/// Media byte
enum media_t {
	MEDIA_2DD_10 = 0xE0,	// 2DD/10 sector
	MEDIA_1D_9 = 0xE5,		// 1D/9 sector
	MEDIA_2D_9 = 0xE6,		// 2D/9 sector
	MEDIA_1D_8 = 0xE7,		// 1D/8 sector
	MEDIA_2D_8 = 0xE8,		// 2D/8 sector
	MEDIA_2HT = 0xEA,		// 2HT
	MEDIA_2HS = 0xEB,		// 2HS
	MEDIA_2HDE = 0xEC,		// 2DDE
	MEDIA_1DD_9 = 0xEE,		// 1DD/9 sector
	MEDIA_1DD_8 = 0xEF,		// 1DD/8 sector
	MEDIA_MANUAL = 0xF1,	// Remote drive (manual eject)
	MEDIA_REMOVABLE = 0xF2,	// Remote drive (removable)
	MEDIA_REMOTE = 0xF3,	// Remote drive
	MEDIA_DAT = 0xF4,		// SCSI-DAT
	MEDIA_CDROM = 0xF5,		// SCSI-CDROM
	MEDIA_MO = 0xF6,		// SCSI-MO
	MEDIA_SCSI_HD = 0xF7,	// SCSI-HD
	MEDIA_SASI_HD = 0xF8,	// SASI-HD
	MEDIA_RAMDISK = 0xF9,	// RAM disk
	MEDIA_2HQ = 0xFA,		// 2HQ
	MEDIA_2DD_8 = 0xFB,		// 2DD/8 sector
	MEDIA_2DD_9 = 0xFC,		// 2DD/9 sector
	MEDIA_2HC = 0xFD,		// 2HC
	MEDIA_2HD = 0xFE,		// 2HD
};

/// namests struct
typedef struct {
	BYTE wildcard;			// Wildcard character length
	BYTE drive;				// Drive number
	BYTE path[65];			// Path (subdirectory +/)
	BYTE name[8];			// File name (PADDING 0x20)
	BYTE ext[3];			// Extension (PADDING 0x20)
	BYTE add[10];			// File name addition (PADDING 0x00)
} namests_t;

/// files struct
typedef struct {
	BYTE fatr;				// + 0 search attribute; read-only
	BYTE pad1[3];			// padding
//		BYTE drive;			// + 1 drive number; read-only
	DWORD sector;			// + 2 directory sector; DOS _FILES first address substitute
//		WORD cluster;		// + 6 directory cluster; details unknown (unused)
	WORD offset;			// + 8 directory entry; write-only
//		BYTE name[8];		// +10 working file name; write-only (unused)
//		BYTE ext[3];		// +18 working extension; write-only (unused)
	BYTE attr;				// +21 file attribute; write-only
	BYTE pad2;				// padding
	WORD time;				// +22 last change time of day; write-only
	WORD date;				// +24 last change date; write-only
	DWORD size;				// +26 file size; write-only
	BYTE full[23];			// +30 full name; write-only
	BYTE pad3;				// padding
} files_t;

/// FCBç\ë¢ëÃ
typedef struct {
//		BYTE pad00[6];	// + 0~+ 5	(unused)
	DWORD fileptr;		// + 6~+ 9	file pointer
//		BYTE pad01[4];	// +10~+13	(unused)
	WORD mode;			// +14~+15	open mode
//		BYTE pad02[16];	// +16~+31	(unused)
//		DWORD zero;		// +32~+35	zeros are written when opened (unused)
//		BYTE name[8];	// +36~+43	file name (PADDING 0x20) (unused)
//		BYTE ext[3];	// +44~+46	extension (PADDING 0x20) (unused)
	BYTE attr;			// +47		file attribute
//		BYTE add[10];	// +48~+57	file name addition (PADDING 0x00) (unused)
	WORD time;			// +58~+59	last change time of day
	WORD date;			// +60~+61	last change date
//		WORD cluster;	// +62~+63	cluster number (unused)
	DWORD size;			// +64~+67	file size
//		BYTE pad03[28];	// +68~+95	FAT cache (unused)
} fcb_t;

/// capacity struct
typedef struct {
	WORD freearea;		// + 0	Number of available clusters
	WORD clusters;		// + 2	Total number of clusters
	WORD sectors;		// + 4	Number of sectors per cluster
	WORD bytes;			// + 6	Number of bytes per sector
} capacity_t;

/// ctrldrive struct
typedef struct {
	BYTE status;			// +13	status
	BYTE pad[3];			// padding
} ctrldrive_t;

/// DPB struct
typedef struct {
	WORD sector_size;		// + 0	Number of bytes in one sector
	BYTE cluster_size;		// + 2	Number sectors in one cluster -1
	BYTE shift;				// + 3	Number of clusterÅ®sector shifts
	WORD fat_sector;		// + 4	FAT first sector number
	BYTE fat_max;			// + 6	FAT storage quantity
	BYTE fat_size;			// + 7	FAT controlled sector number (excluding duplicates)
	WORD file_max;			// + 8	Number of files in the root directory
	WORD data_sector;		// +10	First sector number of data storage
		WORD cluster_max;	// +12	Total number of clusters +1
		WORD root_sector;	// +14	First sector number of root directory
//	DWORD driverentry;		// +16	Device driver pointer (unused)
		BYTE media;			// +20	Media identifier
//	BYTE flag;				// +21	Flag used by DPB (unused)
		BYTE pad;			// padding
} dpb_t;

/// Directory entry struct
typedef struct {
	BYTE name[8];			// + 0	File name (PADDING 0x20)
	BYTE ext[3];			// + 8	Extension (PADDING 0x20)
	BYTE attr;				// +11	File attribute
	BYTE add[10];			// +12	File name addition (PADDING 0x00)
	WORD time;				// +22	Last change time of day
	WORD date;				// +24	Last change date
	WORD cluster;			// +26	Cluster number
	DWORD size;				// +28	File size
} dirent_t;

/// IOCTRL parameter union
typedef union {
	BYTE buffer[8];			// Access in byte units
	DWORD param;			// Parameter (First 4 bytes)
	WORD media;				// Media byte (First 2 bytes)
} ioctrl_t;

/// Command line parameter struct
typedef struct {
	BYTE buf[256];			// Command line argument
} argument_t;

#endif	// bridge_h
