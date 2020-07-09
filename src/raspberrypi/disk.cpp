//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	XM6i
//	Copyright (C) 2010-2015 isaki@NetBSD.org
//	Copyright (C) 2010 Y.Sugahara
//
//	Imported sava's Anex86/T98Next image and MO format support patch.
//	Imported NetBSD support and some optimisation patch by Rin Okuyama.
//  Comments translated to english by akuker.
//
//	[ Disk ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"
#ifdef RASCSI
#include "gpiobus.h"
#ifndef BAREMETAL
#include "ctapdriver.h"
#endif	// BAREMETAL
#include "cfilesystem.h"
#include "disk.h"
#else
#include "vm.h"
#include "disk.h"
#include "windrv.h"
#include "ctapdriver.h"
#include "mfc_com.h"
#include "mfc_host.h"
#endif	// RASCSI

//===========================================================================
//
//	Disk
//
//===========================================================================
//#define DISK_LOG

#ifdef RASCSI
#define BENDER_SIGNATURE "SONY    "
#else
#define BENDER_SIGNATURE "XM6"
#endif

//===========================================================================
//
//	Disk Track
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
DiskTrack::DiskTrack()
{
	// Initialization of internal information
	dt.track = 0;
	dt.size = 0;
	dt.sectors = 0;
	dt.raw = FALSE;
	dt.init = FALSE;
	dt.changed = FALSE;
	dt.length = 0;
	dt.buffer = NULL;
	dt.maplen = 0;
	dt.changemap = NULL;
	dt.imgoffset = 0;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
DiskTrack::~DiskTrack()
{
	// Release memory, but do not save automatically
	if (dt.buffer) {
		free(dt.buffer);
		dt.buffer = NULL;
	}
	if (dt.changemap) {
		free(dt.changemap);
		dt.changemap = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
void FASTCALL DiskTrack::Init(
	int track, int size, int sectors, BOOL raw, off64_t imgoff)
{
	ASSERT(track >= 0);
	ASSERT((size >= 8) && (size <= 11));
	ASSERT((sectors > 0) && (sectors <= 0x100));
	ASSERT(imgoff >= 0);

	// Set Parameters
	dt.track = track;
	dt.size = size;
	dt.sectors = sectors;
	dt.raw = raw;

	// Not initialized (needs to be loaded)
	dt.init = FALSE;

	// Not Changed
	dt.changed = FALSE;

	// Offset to actual data
	dt.imgoffset = imgoff;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Load(const Filepath& path)
{
	Fileio fio;
	off64_t offset;
	int i;
	int length;

	ASSERT(this);

	// Not needed if already loaded
	if (dt.init) {
		ASSERT(dt.buffer);
		ASSERT(dt.changemap);
		return TRUE;
	}

	// Calculate offset (previous tracks are considered to
    // hold 256 sectors)
	offset = ((off64_t)dt.track << 8);
	if (dt.raw) {
		ASSERT(dt.size == 11);
		offset *= 0x930;
		offset += 0x10;
	} else {
		offset <<= dt.size;
	}

	// Add offset to real image
	offset += dt.imgoffset;

	// Calculate length (data size of this track)
	length = dt.sectors << dt.size;

	// Allocate buffer memory
	ASSERT((dt.size >= 8) && (dt.size <= 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));

	if (dt.buffer == NULL) {
#if defined(RASCSI) && !defined(BAREMETAL)
		posix_memalign((void **)&dt.buffer, 512, ((length + 511) / 512) * 512);
#else
		dt.buffer = (BYTE *)malloc(length * sizeof(BYTE));
#endif	// RASCSI && !BAREMETAL
		dt.length = length;
	}

	if (!dt.buffer) {
		return FALSE;
	}

	// Reallocate if the buffer length is different
	if (dt.length != (DWORD)length) {
		free(dt.buffer);
#if defined(RASCSI) && !defined(BAREMETAL)
		posix_memalign((void **)&dt.buffer, 512, ((length + 511) / 512) * 512);
#else
		dt.buffer = (BYTE *)malloc(length * sizeof(BYTE));
#endif	// RASCSI && !BAREMETAL
		dt.length = length;
	}

	// Reserve change map memory
	if (dt.changemap == NULL) {
		dt.changemap = (BOOL *)malloc(dt.sectors * sizeof(BOOL));
		dt.maplen = dt.sectors;
	}

	if (!dt.changemap) {
		return FALSE;
	}

	// Reallocate if the buffer length is different
	if (dt.maplen != (DWORD)dt.sectors) {
		free(dt.changemap);
		dt.changemap = (BOOL *)malloc(dt.sectors * sizeof(BOOL));
		dt.maplen = dt.sectors;
	}

	// Clear changemap
	memset(dt.changemap, 0x00, dt.sectors * sizeof(BOOL));

	// Read from File
#if defined(RASCSI) && !defined(BAREMETAL)
	if (!fio.OpenDIO(path, Fileio::ReadOnly)) {
#else
	if (!fio.Open(path, Fileio::ReadOnly)) {
#endif	// RASCSI && !BAREMETAL
		return FALSE;
	}
	if (dt.raw) {
		// Split Reading
		for (i = 0; i < dt.sectors; i++) {
			// Seek
			if (!fio.Seek(offset)) {
				fio.Close();
				return FALSE;
			}

			// Read
			if (!fio.Read(&dt.buffer[i << dt.size], 1 << dt.size)) {
				fio.Close();
				return FALSE;
			}

			// Next offset
			offset += 0x930;
		}
	} else {
		// Continuous reading
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}
		if (!fio.Read(dt.buffer, length)) {
			fio.Close();
			return FALSE;
		}
	}
	fio.Close();

	// Set a flag and end normally
	dt.init = TRUE;
	dt.changed = FALSE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Save(const Filepath& path)
{
	off64_t offset;
	int i;
	int j;
	Fileio fio;
	int length;
	int total;

	ASSERT(this);

	// Not needed if not initialized
	if (!dt.init) {
		return TRUE;
	}

	// Not needed unless changed
	if (!dt.changed) {
		return TRUE;
	}

	// Need to write
	ASSERT(dt.buffer);
	ASSERT(dt.changemap);
	ASSERT((dt.size >= 8) && (dt.size <= 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));

	// Writing in RAW mode is not allowed
	ASSERT(!dt.raw);

	// Calculate offset (previous tracks are considered to hold 256
	offset = ((off64_t)dt.track << 8);
	offset <<= dt.size;

	// Add offset to real image
	offset += dt.imgoffset;

	// Calculate length per sector
	length = 1 << dt.size;

	// Open file
	if (!fio.Open(path, Fileio::ReadWrite)) {
		return FALSE;
	}

	// Partial write loop
	for (i = 0; i < dt.sectors;) {
		// If changed
		if (dt.changemap[i]) {
			// Initialize write size
			total = 0;

			// Seek
			if (!fio.Seek(offset + ((off64_t)i << dt.size))) {
				fio.Close();
				return FALSE;
			}

			// Consectutive sector length
			for (j = i; j < dt.sectors; j++) {
				// end when interrupted
				if (!dt.changemap[j]) {
					break;
				}

				// Add one sector
				total += length;
			}

			// Write
			if (!fio.Write(&dt.buffer[i << dt.size], total)) {
				fio.Close();
				return FALSE;
			}

			// To unmodified sector
			i = j;
		} else {
			// Next Sector
			i++;
		}
	}

	// Close
	fio.Close();

	// Drop the change flag and exit
	memset(dt.changemap, 0x00, dt.sectors * sizeof(BOOL));
	dt.changed = FALSE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Read Sector
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Read(BYTE *buf, int sec) const
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT((sec >= 0) & (sec < 0x100));

	// Error if not initialized
	if (!dt.init) {
		return FALSE;
	}

	// // Error if the number of sectors exceeds the valid number
	if (sec >= dt.sectors) {
		return FALSE;
	}

	// Copy
	ASSERT(dt.buffer);
	ASSERT((dt.size >= 8) && (dt.size <= 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
	memcpy(buf, &dt.buffer[(off64_t)sec << dt.size], (off64_t)1 << dt.size);

	// Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Write Sector
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Write(const BYTE *buf, int sec)
{
	int offset;
	int length;

	ASSERT(this);
	ASSERT(buf);
	ASSERT((sec >= 0) & (sec < 0x100));
	ASSERT(!dt.raw);

	// Error if not initialized
	if (!dt.init) {
		return FALSE;
	}

	// // Error if the number of sectors exceeds the valid number
	if (sec >= dt.sectors) {
		return FALSE;
	}

	// Calculate offset and length
	offset = sec << dt.size;
	length = 1 << dt.size;

	// Compare
	ASSERT(dt.buffer);
	ASSERT((dt.size >= 8) && (dt.size <= 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
	if (memcmp(buf, &dt.buffer[offset], length) == 0) {
		// 同じものを書き込もうとしているので、正常終了
		return TRUE;
	}

	// Copy, change
	memcpy(&dt.buffer[offset], buf, length);
	dt.changemap[sec] = TRUE;
	dt.changed = TRUE;

	// Success
	return TRUE;
}

//===========================================================================
//
//	Disk Cache
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
DiskCache::DiskCache(
	const Filepath& path, int size, int blocks, off64_t imgoff)
{
	int i;

	ASSERT((size >= 8) && (size <= 11));
	ASSERT(blocks > 0);
	ASSERT(imgoff >= 0);

	// Cache work
	for (i = 0; i < CacheMax; i++) {
		cache[i].disktrk = NULL;
		cache[i].serial = 0;
	}

	// Other
	serial = 0;
	sec_path = path;
	sec_size = size;
	sec_blocks = blocks;
	cd_raw = FALSE;
	imgoffset = imgoff;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
DiskCache::~DiskCache()
{
	// Clear the track
	Clear();
}

//---------------------------------------------------------------------------
//
//	RAW Mode Setting
//
//---------------------------------------------------------------------------
void FASTCALL DiskCache::SetRawMode(BOOL raw)
{
	ASSERT(this);
	ASSERT(sec_size == 11);

	// Configuration
	cd_raw = raw;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Save()
{
	int i;

	ASSERT(this);

	// Save track
	for (i = 0; i < CacheMax; i++) {
		// Is it a valid track?
		if (cache[i].disktrk) {
			// Save
			if (!cache[i].disktrk->Save(sec_path)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get disk cache information
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::GetCache(int index, int& track, DWORD& aserial) const
{
	ASSERT(this);
	ASSERT((index >= 0) && (index < CacheMax));

	// FALSE if unused
	if (!cache[index].disktrk) {
		return FALSE;
	}

	// Set track and serial
	track = cache[index].disktrk->GetTrack();
	aserial = cache[index].serial;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Clear
//
//---------------------------------------------------------------------------
void FASTCALL DiskCache::Clear()
{
	int i;

	ASSERT(this);

	// Free the cache
	for (i = 0; i < CacheMax; i++) {
		if (cache[i].disktrk) {
			delete cache[i].disktrk;
			cache[i].disktrk = NULL;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Sector Read
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Read(BYTE *buf, int block)
{
	int track;
	DiskTrack *disktrk;

	ASSERT(this);
	ASSERT(sec_size != 0);

	// Update first
	Update();

	// Calculate track (fixed to 256 sectors/track)
	track = block >> 8;

	// Get the track data
	disktrk = Assign(track);
	if (!disktrk) {
		return FALSE;
	}

	// Read the track data to the cache
	return disktrk->Read(buf, (BYTE)block);
}

//---------------------------------------------------------------------------
//
//	Sector write
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Write(const BYTE *buf, int block)
{
	int track;
	DiskTrack *disktrk;

	ASSERT(this);
	ASSERT(sec_size != 0);

	// Update first
	Update();

	// Calculate track (fixed to 256 sectors/track)
	track = block >> 8;

	// Get that track data
	disktrk = Assign(track);
	if (!disktrk) {
		return FALSE;
	}

	// Write the data to the cache
	return disktrk->Write(buf, (BYTE)block);
}

//---------------------------------------------------------------------------
//
//	Track Assignment
//
//---------------------------------------------------------------------------
DiskTrack* FASTCALL DiskCache::Assign(int track)
{
	int i;
	int c;
	DWORD s;
	DiskTrack *disktrk;

	ASSERT(this);
	ASSERT(sec_size != 0);
	ASSERT(track >= 0);

	// First, check if it is already assigned
	for (i = 0; i < CacheMax; i++) {
		if (cache[i].disktrk) {
			if (cache[i].disktrk->GetTrack() == track) {
				// Track match
				cache[i].serial = serial;
				return cache[i].disktrk;
			}
		}
	}

	// Next, check for empty
	for (i = 0; i < CacheMax; i++) {
		if (!cache[i].disktrk) {
			// Try loading
			if (Load(i, track)) {
				// Success loading
				cache[i].serial = serial;
				return cache[i].disktrk;
			}

			// Load failed
			return NULL;
		}
	}

	// Finally, find the youngest serial number and delete it

	// Set index 0 as candidate c
	s = cache[0].serial;
	c = 0;

	// Compare candidate with serial and update to smaller one
	for (i = 0; i < CacheMax; i++) {
		ASSERT(cache[i].disktrk);

		// Compare and update the existing serial
		if (cache[i].serial < s) {
			s = cache[i].serial;
			c = i;
		}
	}

	// Save this track
	if (!cache[c].disktrk->Save(sec_path)) {
		return NULL;
	}

	// Delete this track
	disktrk = cache[c].disktrk;
	cache[c].disktrk = NULL;

	// Load
	if (Load(c, track, disktrk)) {
		// Successful loading
		cache[c].serial = serial;
		return cache[c].disktrk;
	}

	// Load failed
	return NULL;
}

//---------------------------------------------------------------------------
//
//	Load cache
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Load(int index, int track, DiskTrack *disktrk)
{
	int sectors;

	ASSERT(this);
	ASSERT((index >= 0) && (index < CacheMax));
	ASSERT(track >= 0);
	ASSERT(!cache[index].disktrk);

	// Get the number of sectors on this track
	sectors = sec_blocks - (track << 8);
	ASSERT(sectors > 0);
	if (sectors > 0x100) {
		sectors = 0x100;
	}

	// Create a disk track
	if (disktrk == NULL) {
		disktrk = new DiskTrack();
	}

	// Initialize disk track
	disktrk->Init(track, sec_size, sectors, cd_raw, imgoffset);

	// Try loading
	if (!disktrk->Load(sec_path)) {
		// 失敗
		delete disktrk;
		return FALSE;
	}

	// Allocation successful, work set
	cache[index].disktrk = disktrk;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Update serial number
//
//---------------------------------------------------------------------------
void FASTCALL DiskCache::Update()
{
	int i;

	ASSERT(this);

	// Update and do nothing except 0
	serial++;
	if (serial != 0) {
		return;
	}

	// Clear serial of all caches (loop in 32bit)
	for (i = 0; i < CacheMax; i++) {
		cache[i].serial = 0;
	}
}


//===========================================================================
//
//	Disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Disk::Disk()
{
	// Work initialization
	disk.id = MAKEID('N', 'U', 'L', 'L');
	disk.ready = FALSE;
	disk.writep = FALSE;
	disk.readonly = FALSE;
	disk.removable = FALSE;
	disk.lock = FALSE;
	disk.attn = FALSE;
	disk.reset = FALSE;
	disk.size = 0;
	disk.blocks = 0;
	disk.lun = 0;
	disk.code = 0;
	disk.dcache = NULL;
	disk.imgoffset = 0;

	// Other
	cache_wb = TRUE;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
Disk::~Disk()
{
	// Save disk cache
	if (disk.ready) {
		// Only if ready...
		if (disk.dcache) {
			disk.dcache->Save();
		}
	}

	// Clear disk cache
	if (disk.dcache) {
		delete disk.dcache;
		disk.dcache = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Disk::Reset()
{
	ASSERT(this);

	// no lock, no attention, reset
	disk.lock = FALSE;
	disk.attn = FALSE;
	disk.reset = TRUE;
}

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Save(Fileio *fio, int ver)
{
	DWORD sz;
	DWORD padding;

	ASSERT(this);
	ASSERT(fio);

	// Save size
	sz = 52;
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save entity
	PROP_EXPORT(fio, disk.id);
	PROP_EXPORT(fio, disk.ready);
	PROP_EXPORT(fio, disk.writep);
	PROP_EXPORT(fio, disk.readonly);
	PROP_EXPORT(fio, disk.removable);
	PROP_EXPORT(fio, disk.lock);
	PROP_EXPORT(fio, disk.attn);
	PROP_EXPORT(fio, disk.reset);
	PROP_EXPORT(fio, disk.size);
	PROP_EXPORT(fio, disk.blocks);
	PROP_EXPORT(fio, disk.lun);
	PROP_EXPORT(fio, disk.code);
	PROP_EXPORT(fio, padding);

	// Save the path
	if (!diskpath.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Load(Fileio *fio, int ver)
{
	DWORD sz;
	disk_t buf;
	DWORD padding;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);

	// Prior to version 2.03, the disk was not saved
	if (ver <= 0x0202) {
		return TRUE;
	}

	// Delete the current disk cache
	if (disk.dcache) {
		disk.dcache->Save();
		delete disk.dcache;
		disk.dcache = NULL;
	}

	// Load size
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != 52) {
		return FALSE;
	}

	// Load into buffer
	PROP_IMPORT(fio, buf.id);
	PROP_IMPORT(fio, buf.ready);
	PROP_IMPORT(fio, buf.writep);
	PROP_IMPORT(fio, buf.readonly);
	PROP_IMPORT(fio, buf.removable);
	PROP_IMPORT(fio, buf.lock);
	PROP_IMPORT(fio, buf.attn);
	PROP_IMPORT(fio, buf.reset);
	PROP_IMPORT(fio, buf.size);
	PROP_IMPORT(fio, buf.blocks);
	PROP_IMPORT(fio, buf.lun);
	PROP_IMPORT(fio, buf.code);
	PROP_IMPORT(fio, padding);

	// Load path
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// Move only if IDs match
	if (disk.id == buf.id) {
		// Do nothing if null
		if (IsNULL()) {
			return TRUE;
		}

		// Same type of device as when saving
		disk.ready = FALSE;
		if (Open(path)) {
            // Disk cache is created in Open
            // move only properties
			if (!disk.readonly) {
				disk.writep = buf.writep;
			}
			disk.lock = buf.lock;
			disk.attn = buf.attn;
			disk.reset = buf.reset;
			disk.lun = buf.lun;
			disk.code = buf.code;

			// Loaded successfully
			return TRUE;
		}
	}

	// Disk cache rebuild
	if (!IsReady()) {
		disk.dcache = NULL;
	} else {
		disk.dcache = new DiskCache(diskpath, disk.size, disk.blocks);
	}

	return TRUE;
}
#endif	// RASCSI

//---------------------------------------------------------------------------
//
//	NULL Check
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::IsNULL() const
{
	ASSERT(this);

	if (disk.id == MAKEID('N', 'U', 'L', 'L')) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	SASI Check
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::IsSASI() const
{
	ASSERT(this);

	if (disk.id == MAKEID('S', 'A', 'H', 'D')) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Open
//  * Call as a post-process after successful opening in a derived class
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Open(const Filepath& path, BOOL /*attn*/)
{
	Fileio fio;

	ASSERT(this);
	ASSERT((disk.size >= 8) && (disk.size <= 11));
	ASSERT(disk.blocks > 0);

	// Ready
	disk.ready = TRUE;

	// Cache initialization
	ASSERT(!disk.dcache);
	disk.dcache =
		new DiskCache(path, disk.size, disk.blocks, disk.imgoffset);

	// Can read/write open
	if (fio.Open(path, Fileio::ReadWrite)) {
		// Write permission, not read only
		disk.writep = FALSE;
		disk.readonly = FALSE;
		fio.Close();
	} else {
		// Write protected, read only
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// Not locked
	disk.lock = FALSE;

	// Save path
	diskpath = path;

	// Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Eject
//
//---------------------------------------------------------------------------
void FASTCALL Disk::Eject(BOOL force)
{
	ASSERT(this);

	// Can only be ejected if it is removable
	if (!disk.removable) {
		return;
	}

	// If you're not ready, you don't need to eject
	if (!disk.ready) {
		return;
	}

	// Must be unlocked if there is no force flag
	if (!force) {
		if (disk.lock) {
			return;
		}
	}

	// Remove disk cache
	disk.dcache->Save();
	delete disk.dcache;
	disk.dcache = NULL;

	// Not ready, no attention
	disk.ready = FALSE;
	disk.writep = FALSE;
	disk.readonly = FALSE;
	disk.attn = FALSE;
}

//---------------------------------------------------------------------------
//
//	Write Protected
//
//---------------------------------------------------------------------------
void FASTCALL Disk::WriteP(BOOL writep)
{
	ASSERT(this);

	// be ready
	if (!disk.ready) {
		return;
	}

	// Read Only, protect only
	if (disk.readonly) {
		ASSERT(disk.writep);
		return;
	}

	// Write protect flag setting
	disk.writep = writep;
}

//---------------------------------------------------------------------------
//
//	Get Disk
//
//---------------------------------------------------------------------------
void FASTCALL Disk::GetDisk(disk_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	// Assign internal buffer
	*buffer = disk;
}

//---------------------------------------------------------------------------
//
//	Get Path
//
//---------------------------------------------------------------------------
void FASTCALL Disk::GetPath(Filepath& path) const
{
	path = diskpath;
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Flush()
{
	ASSERT(this);

	// Do nothing if there's nothing cached
	if (!disk.dcache) {
		return TRUE;
	}

	// Save cache
	return disk.dcache->Save();
}

//---------------------------------------------------------------------------
//
//	Check Ready
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::CheckReady()
{
	ASSERT(this);

	// Not ready if reset
	if (disk.reset) {
		disk.code = DISK_DEVRESET;
		disk.reset = FALSE;
		return FALSE;
	}

	// Not ready if it needs attention
	if (disk.attn) {
		disk.code = DISK_ATTENTION;
		disk.attn = FALSE;
		return FALSE;
	}

	// Return status if not ready
	if (!disk.ready) {
		disk.code = DISK_NOTREADY;
		return FALSE;
	}

	// Initialization with no error
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//	*You need to be successful at all times
//
//---------------------------------------------------------------------------
int FASTCALL Disk::Inquiry(
	const DWORD* /*cdb*/, BYTE* /*buf*/, DWORD /*major*/, DWORD /*minor*/)
{
	ASSERT(this);

	// default is INQUIRY failure
	disk.code = DISK_INVALIDCMD;
	return 0;
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//	*SASI is a separate process
//
//---------------------------------------------------------------------------
int FASTCALL Disk::RequestSense(const DWORD *cdb, BYTE *buf)
{
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);

	// Return not ready only if there are no errors
	if (disk.code == DISK_NOERROR) {
		if (!disk.ready) {
			disk.code = DISK_NOTREADY;
		}
	}

	// Size determination (according to allocation length)
	size = (int)cdb[4];
	ASSERT((size >= 0) && (size < 0x100));

	// For SCSI-1, transfer 4 bytes when the size is 0
    // (Deleted this specification for SCSI-2)
	if (size == 0) {
		size = 4;
	}

	// Clear the buffer
	memset(buf, 0, size);

	// Set 18 bytes including extended sense data
	buf[0] = 0x70;
	buf[2] = (BYTE)(disk.code >> 16);
	buf[7] = 10;
	buf[12] = (BYTE)(disk.code >> 8);
	buf[13] = (BYTE)disk.code;

	// Clear the code
	disk.code = 0x00;

	return size;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT check
//	*Not affected by disk.code
//
//---------------------------------------------------------------------------
int FASTCALL Disk::SelectCheck(const DWORD *cdb)
{
	int length;

	ASSERT(this);
	ASSERT(cdb);

	// Error if save parameters are set instead of SCSIHD
	if (disk.id != MAKEID('S', 'C', 'H', 'D')) {
		// Error if save parameters are set
		if (cdb[1] & 0x01) {
			disk.code = DISK_INVALIDCDB;
			return 0;
		}
	}

	// Receive the data specified by the parameter length
	length = (int)cdb[4];
	return length;
}


//---------------------------------------------------------------------------
//
//	MODE SELECT(10) check
//	* Not affected by disk.code
//
//---------------------------------------------------------------------------
int FASTCALL Disk::SelectCheck10(const DWORD *cdb)
{
	DWORD length;

	ASSERT(this);
	ASSERT(cdb);

	// Error if save parameters are set instead of SCSIHD
	if (disk.id != MAKEID('S', 'C', 'H', 'D')) {
		if (cdb[1] & 0x01) {
			disk.code = DISK_INVALIDCDB;
			return 0;
		}
	}

	// Receive the data specified by the parameter length
	length = cdb[7];
	length <<= 8;
	length |= cdb[8];
	if (length > 0x800) {
		length = 0x800;
	}

	return (int)length;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//	* Not affected by disk.code
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::ModeSelect(
	const DWORD* /*cdb*/, const BYTE *buf, int length)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(length >= 0);

	// cannot be set
	disk.code = DISK_INVALIDPRM;

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	MODE SENSE
//	*Not affected by disk.code
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ModeSense(const DWORD *cdb, BYTE *buf)
{
	int page;
	int length;
	int size;
	BOOL valid;
	BOOL change;
	int ret;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x1a);

	// Get length, clear buffer
	length = (int)cdb[4];
	ASSERT((length >= 0) && (length < 0x100));
	memset(buf, 0, length);

	// Get changeable flag
	if ((cdb[2] & 0xc0) == 0x40) {
        //** printf("MODESENSE: Change = TRUE\n");
		change = TRUE;
	} else {
        //** printf("MODESENSE: Change = FALSE\n");
		change = FALSE;
	}

	// Get page code (0x00 is valid from the beginning)
	page = cdb[2] & 0x3f;
	if (page == 0x00) {
        //** printf("MODESENSE: Page code: OK %02X\n", cdb[2]);
		valid = TRUE;
	} else {
        //** printf("MODESENSE: Invalid page code received %02X\n", cdb[2]);
		valid = FALSE;
	}

	// Basic information
	size = 4;

	// MEDIUM TYPE
	if (disk.id == MAKEID('S', 'C', 'M', 'O')) {
		buf[1] = 0x03; // optical reversible or erasable
	}

	// DEVICE SPECIFIC PARAMETER
	if (disk.writep) {
        //** printf("MODESENSE: Write protect\n");
		buf[2] = 0x80;
	}

	// add block descriptor if DBD is 0
	if ((cdb[1] & 0x08) == 0) {
		// Mode parameter header
		buf[3] = 0x08;

		// Only if ready
		if (disk.ready) {
            //** printf("MODESENSE: Disk is ready\n");
			// Block descriptor (number of blocks)
			buf[5] = (BYTE)(disk.blocks >> 16);
			buf[6] = (BYTE)(disk.blocks >> 8);
			buf[7] = (BYTE)disk.blocks;

			// Block descriptor (block length)
			size = 1 << disk.size;
			buf[9] = (BYTE)(size >> 16);
			buf[10] = (BYTE)(size >> 8);
			buf[11] = (BYTE)size;
		}

		// size
		size = 12;
	}

	// Page code 1(read-write error recovery)
	if ((page == 0x01) || (page == 0x3f)) {
		size += AddError(change, &buf[size]);
		valid = TRUE;
	}

	// Page code 3(format device)
	if ((page == 0x03) || (page == 0x3f)) {
		size += AddFormat(change, &buf[size]);
		valid = TRUE;
	}

	// Page code 4(drive parameter)
	if ((page == 0x04) || (page == 0x3f)) {
		size += AddDrive(change, &buf[size]);
		valid = TRUE;
	}

	// Page code 6(optical)
	if (disk.id == MAKEID('S', 'C', 'M', 'O')) {
		if ((page == 0x06) || (page == 0x3f)) {
			size += AddOpt(change, &buf[size]);
			valid = TRUE;
		}
	}

	// Page code 8(caching)
	if ((page == 0x08) || (page == 0x3f)) {
		size += AddCache(change, &buf[size]);
		valid = TRUE;
	}

	// Page code 13(CD-ROM)
	if (disk.id == MAKEID('S', 'C', 'C', 'D')) {
		if ((page == 0x0d) || (page == 0x3f)) {
			size += AddCDROM(change, &buf[size]);
			valid = TRUE;
		}
	}

	// Page code 14(CD-DA)
	if (disk.id == MAKEID('S', 'C', 'C', 'D')) {
		if ((page == 0x0e) || (page == 0x3f)) {
			size += AddCDDA(change, &buf[size]);
			valid = TRUE;
		}
	}

	// Page (vendor special)
	ret = AddVendor(page, change, &buf[size]);
	if (ret > 0) {
		size += ret;
		valid = TRUE;
	}

	// final setting of mode data length
	buf[0] = (BYTE)(size - 1);

	// Unsupported page
	if (!valid) {
	//** printf("MODESENSE: Something was invalid...\n");
		disk.code = DISK_INVALIDCDB;
		return 0;
	}

	//** printf("MODESENSE: mode sense length is %d\n",length);

	// MODE SENSE success
	disk.code = DISK_NOERROR;
	return length;
}

//---------------------------------------------------------------------------
//
//	MODE SENSE(10)
//	*Not affected by disk.code
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ModeSense10(const DWORD *cdb, BYTE *buf)
{
	int page;
	int length;
	int size;
	BOOL valid;
	BOOL change;
	int ret;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x5a);

	// Get length, clear buffer
	length = cdb[7];
	length <<= 8;
	length |= cdb[8];
	if (length > 0x800) {
		length = 0x800;
	}
	ASSERT((length >= 0) && (length < 0x800));
	memset(buf, 0, length);

	// Get changeable flag
	if ((cdb[2] & 0xc0) == 0x40) {
		change = TRUE;
	} else {
		change = FALSE;
	}

	// Get page code (0x00 is valid from the beginning)
	page = cdb[2] & 0x3f;
	if (page == 0x00) {
		valid = TRUE;
	} else {
		valid = FALSE;
	}

	// Basic Information
	size = 4;
	if (disk.writep) {
		buf[2] = 0x80;
	}

	// add block descriptor if DBD is 0
	if ((cdb[1] & 0x08) == 0) {
		// Mode parameter header
		buf[3] = 0x08;

		// Only if ready
		if (disk.ready) {
			// Block descriptor (number of blocks)
			buf[5] = (BYTE)(disk.blocks >> 16);
			buf[6] = (BYTE)(disk.blocks >> 8);
			buf[7] = (BYTE)disk.blocks;

			// Block descriptor (block length)
			size = 1 << disk.size;
			buf[9] = (BYTE)(size >> 16);
			buf[10] = (BYTE)(size >> 8);
			buf[11] = (BYTE)size;
		}

		// Size
		size = 12;
	}

	// Page code 1(read-write error recovery)
	if ((page == 0x01) || (page == 0x3f)) {
		size += AddError(change, &buf[size]);
		valid = TRUE;
	}

	// Page code 3(format device)
	if ((page == 0x03) || (page == 0x3f)) {
		size += AddFormat(change, &buf[size]);
		valid = TRUE;
	}

	// Page code 4(drive parameter)
	if ((page == 0x04) || (page == 0x3f)) {
		size += AddDrive(change, &buf[size]);
		valid = TRUE;
	}

	// ペPage code 6(optical)
	if (disk.id == MAKEID('S', 'C', 'M', 'O')) {
		if ((page == 0x06) || (page == 0x3f)) {
			size += AddOpt(change, &buf[size]);
			valid = TRUE;
		}
	}

	// Page code 8(caching)
	if ((page == 0x08) || (page == 0x3f)) {
		size += AddCache(change, &buf[size]);
		valid = TRUE;
	}

	// Page code 13(CD-ROM)
	if (disk.id == MAKEID('S', 'C', 'C', 'D')) {
		if ((page == 0x0d) || (page == 0x3f)) {
			size += AddCDROM(change, &buf[size]);
			valid = TRUE;
		}
	}

	// Page code 14(CD-DA)
	if (disk.id == MAKEID('S', 'C', 'C', 'D')) {
		if ((page == 0x0e) || (page == 0x3f)) {
			size += AddCDDA(change, &buf[size]);
			valid = TRUE;
		}
	}

	// Page (vendor special)
	ret = AddVendor(page, change, &buf[size]);
	if (ret > 0) {
		size += ret;
		valid = TRUE;
	}

	// final setting of mode data length
	buf[0] = (BYTE)(size - 1);

	// Unsupported page
	if (!valid) {
		disk.code = DISK_INVALIDCDB;
		return 0;
	}

	// MODE SENSE success
	disk.code = DISK_NOERROR;
	return length;
}

//---------------------------------------------------------------------------
//
//	Add error page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddError(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x01;
	buf[1] = 0x0a;

	// No changeable area
	if (change) {
		return 12;
	}

	// Retry count is 0, limit time uses internal default value
	return 12;
}

//---------------------------------------------------------------------------
//
//	Add format page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddFormat(BOOL change, BYTE *buf)
{
	int size;

	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x80 | 0x03;
	buf[1] = 0x16;

	// Show the number of bytes in the physical sector as changeable \
    // (though it cannot be changed in practice)
	if (change) {
		buf[0xc] = 0xff;
		buf[0xd] = 0xff;
		return 24;
	}

	if (disk.ready) {
		// Set the number of tracks in one zone to 8 (TODO)
		buf[0x3] = 0x08;

		// Set sector/track to 25 (TODO)
		buf[0xa] = 0x00;
		buf[0xb] = 0x19;

		// Set the number of bytes in the physical sector
		size = 1 << disk.size;
		buf[0xc] = (BYTE)(size >> 8);
		buf[0xd] = (BYTE)size;
	}

	// Set removable attribute
	if (disk.removable) {
		buf[20] = 0x20;
	}

	return 24;
}

//---------------------------------------------------------------------------
//
//	Add drive page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddDrive(BOOL change, BYTE *buf)
{
	DWORD cylinder;

	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x04;
	buf[1] = 0x16;

	// No changeable area
	if (change) {
		return 24;
	}

	if (disk.ready) {
		// Set the number of cylinders (total number of blocks
        // divided by 25 sectors/track and 8 heads)
		cylinder = disk.blocks;
		cylinder >>= 3;
		cylinder /= 25;
		buf[0x2] = (BYTE)(cylinder >> 16);
		buf[0x3] = (BYTE)(cylinder >> 8);
		buf[0x4] = (BYTE)cylinder;

		// Fix the head at 8
		buf[0x5] = 0x8;
	}

	return 24;
}

//---------------------------------------------------------------------------
//
//	Add option
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddOpt(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x06;
	buf[1] = 0x02;

	// No changeable area
	if (change) {
		return 4;
	}

	// Do not report update blocks
	return 4;
}

//---------------------------------------------------------------------------
//
//	Add Cache Page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddCache(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x08;
	buf[1] = 0x0a;

	// No changeable area
	if (change) {
		return 12;
	}

	// Only read cache is valid, no prefetch
	return 12;
}

//---------------------------------------------------------------------------
//
//	Add CDROM Page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddCDROM(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x0d;
	buf[1] = 0x06;

	// No changeable area
	if (change) {
		return 8;
	}

	// 2 seconds for inactive timer
	buf[3] = 0x05;

	// MSF multiples are 60 and 75 respectively
	buf[5] = 60;
	buf[7] = 75;

	return 8;
}

//---------------------------------------------------------------------------
//
//	CD-DAページ追加
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddCDDA(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x0e;
	buf[1] = 0x0e;

	// No changeable area
	if (change) {
		return 16;
	}

	// Audio waits for operation completion and allows
    // PLAY across multiple tracks
	return 16;
}

//---------------------------------------------------------------------------
//
//	Add special vendor page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddVendor(int /*page*/, BOOL /*change*/, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	return 0;
}

//---------------------------------------------------------------------------
//
//	READ DEFECT DATA(10)
//	*Not affected by disk.code
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ReadDefectData10(const DWORD *cdb, BYTE *buf)
{
	DWORD length;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x37);

	// Get length, clear buffer
	length = cdb[7];
	length <<= 8;
	length |= cdb[8];
	if (length > 0x800) {
		length = 0x800;
	}
	ASSERT((length >= 0) && (length < 0x800));
	memset(buf, 0, length);

	// P/G/FORMAT
	buf[1] = (cdb[1] & 0x18) | 5;
	buf[3] = 8;

	buf[4] = 0xff;
	buf[5] = 0xff;
	buf[6] = 0xff;
	buf[7] = 0xff;

	buf[8] = 0xff;
	buf[9] = 0xff;
	buf[10] = 0xff;
	buf[11] = 0xff;

	// no list
	disk.code = DISK_NODEFECT;
	return 4;
}

//---------------------------------------------------------------------------
//
//	Command
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::TestUnitReady(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// TEST UNIT READY Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	REZERO UNIT
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Rezero(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// REZERO Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	FORMAT UNIT
//	*Opcode $06 for SASI, Opcode $04 for SCSI
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Format(const DWORD *cdb)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// FMTDATA=1 is not supported (but OK if there is no DEFECT LIST)
	if ((cdb[1] & 0x10) != 0 && cdb[4] != 0) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// FORMAT Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	REASSIGN BLOCKS
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Reassign(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// REASSIGN BLOCKS Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
int FASTCALL Disk::Read(BYTE *buf, DWORD block)
{
	ASSERT(this);
	ASSERT(buf);

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		disk.code = DISK_INVALIDLBA;
		return 0;
	}

	// leave it to the cache
	if (!disk.dcache->Read(buf, block)) {
		disk.code = DISK_READFAULT;
		return 0;
	}

	//  Success
	return (1 << disk.size);
}

//---------------------------------------------------------------------------
//
//	WRITE check
//
//---------------------------------------------------------------------------
int FASTCALL Disk::WriteCheck(DWORD block)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		return 0;
	}

	// Error if write protected
	if (disk.writep) {
		disk.code = DISK_WRITEPROTECT;
		return 0;
	}

	//  Success
	return (1 << disk.size);
}

//---------------------------------------------------------------------------
//
//	WRITE
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Write(const BYTE *buf, DWORD block)
{
	ASSERT(this);
	ASSERT(buf);

	// Error if not ready
	if (!disk.ready) {
		disk.code = DISK_NOTREADY;
		return FALSE;
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		disk.code = DISK_INVALIDLBA;
		return FALSE;
	}

	// Error if write protected
	if (disk.writep) {
		disk.code = DISK_WRITEPROTECT;
		return FALSE;
	}

	// Leave it to the cache
	if (!disk.dcache->Write(buf, block)) {
		disk.code = DISK_WRITEFAULT;
		return FALSE;
	}

	//  Success
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	SEEK
//	*Does not check LBA (SASI IOCS)
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Seek(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// SEEK Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ASSIGN
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Assign(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	//  Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	SPECIFY
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Specify(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	//  Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	START STOP UNIT
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::StartStop(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x1b);

	// Look at the eject bit and eject if necessary
	if (cdb[4] & 0x02) {
		if (disk.lock) {
			// Cannot be ejected because it is locked
			disk.code = DISK_PREVENT;
			return FALSE;
		}

		// Eject
		Eject(FALSE);
	}

	// OK
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	SEND DIAGNOSTIC
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::SendDiag(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x1d);

	// Do not support PF bit
	if (cdb[1] & 0x10) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Do not support parameter list
	if ((cdb[3] != 0) || (cdb[4] != 0)) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Always successful
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	PREVENT/ALLOW MEDIUM REMOVAL
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Removal(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x1e);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// Set Lock flag
	if (cdb[4] & 0x01) {
		disk.lock = TRUE;
	} else {
		disk.lock = FALSE;
	}

	// REMOVAL Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	READ CAPACITY
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ReadCapacity(const DWORD* /*cdb*/, BYTE *buf)
{
	DWORD blocks;
	DWORD length;

	ASSERT(this);
	ASSERT(buf);

	// Buffer clear
	memset(buf, 0, 8);

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Create end of logical block address (disk.blocks-1)
	ASSERT(disk.blocks > 0);
	blocks = disk.blocks - 1;
	buf[0] = (BYTE)(blocks >> 24);
	buf[1] = (BYTE)(blocks >> 16);
	buf[2] = (BYTE)(blocks >> 8);
	buf[3] = (BYTE)blocks;

	// Create block length (1 << disk.size)
	length = 1 << disk.size;
	buf[4] = (BYTE)(length >> 24);
	buf[5] = (BYTE)(length >> 16);
	buf[6] = (BYTE)(length >> 8);
	buf[7] = (BYTE)length;

	// return the size
	return 8;
}

//---------------------------------------------------------------------------
//
//	VERIFY
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Verify(const DWORD *cdb)
{
	DWORD record;
	DWORD blocks;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x2f);

	// Get parameters
	record = cdb[2];
	record <<= 8;
	record |= cdb[3];
	record <<= 8;
	record |= cdb[4];
	record <<= 8;
	record |= cdb[5];
	blocks = cdb[7];
	blocks <<= 8;
	blocks |= cdb[8];

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Parameter check
	if (disk.blocks < (record + blocks)) {
		disk.code = DISK_INVALIDLBA;
		return FALSE;
	}

	//  Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ReadToc(const DWORD *cdb, BYTE *buf)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x43);
	ASSERT(buf);

	// This command is not supported
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::PlayAudio(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x45);

	// This command is not supported
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO MSF
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::PlayAudioMSF(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x47);

	// This command is not supported
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO TRACK
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::PlayAudioTrack(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x48);

	// This command is not supported
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//===========================================================================
//
//	SASI Hard Disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SASIHD::SASIHD() : Disk()
{
	// SASI ハードディスク
	disk.id = MAKEID('S', 'A', 'H', 'D');
}

//---------------------------------------------------------------------------
//
//	リセット
//
//---------------------------------------------------------------------------
void FASTCALL SASIHD::Reset()
{
	// ロック状態解除、アテンション解除
	disk.lock = FALSE;
	disk.attn = FALSE;

	// Resetなし、コードをクリア
	disk.reset = FALSE;
	disk.code = 0x00;
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIHD::Open(const Filepath& path, BOOL /*attn*/)
{
	Fileio fio;
	off64_t size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();
	fio.Close();

#if defined(USE_MZ1F23_1024_SUPPORT)
	// MZ-2500/MZ-2800用 MZ-1F23(SASI 20M/セクタサイズ1024)専用
	// 20M(22437888 BS=1024 C=21912)
	if (size == 0x1566000) {
		// セクタサイズとブロック数
		disk.size = 10;
		disk.blocks = (DWORD)(size >> 10);

		// Call the base class
		return Disk::Open(path);
	}
#endif	// USE_MZ1F23_1024_SUPPORT

#if defined(REMOVE_FIXED_SASIHD_SIZE)
	// 256バイト単位であること
	if (size & 0xff) {
		return FALSE;
	}

	// 10MB以上
	if (size < 0x9f5400) {
		return FALSE;
	}

	// 512MB程度に制限しておく
	if (size > 512 * 1024 * 1024) {
		return FALSE;
	}
#else
	// 10MB, 20MB, 40MBのみ
	switch (size) {
		// 10MB(10441728 BS=256 C=40788)
		case 0x9f5400:
			break;

		// 20MB(20748288 BS=256 C=81048)
		case 0x13c9800:
			break;

		// 40MB(41496576 BS=256 C=162096)
		case 0x2793000:
			break;

		// Other(サポートしない)
		default:
			return FALSE;
	}
#endif	// REMOVE_FIXED_SASIHD_SIZE

	// セクタサイズとブロック数
	disk.size = 8;
	disk.blocks = (DWORD)(size >> 8);

	// Call the base class
	return Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
int FASTCALL SASIHD::RequestSense(const DWORD *cdb, BYTE *buf)
{
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);

	// サイズ決定
	size = (int)cdb[4];
	ASSERT((size >= 0) && (size < 0x100));

	// サイズ0のときに4バイト転送する(Shugart Associates System Interface仕様)
	if (size == 0) {
		size = 4;
	}

	// SASIは非拡張フォーマットに固定
	memset(buf, 0, size);
	buf[0] = (BYTE)(disk.code >> 16);
	buf[1] = (BYTE)(disk.lun << 5);

	// コードをクリア
	disk.code = 0x00;

	return size;
}

//===========================================================================
//
//	SCSI ハードディスク
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIHD::SCSIHD() : Disk()
{
	// SCSI Hard Disk
	disk.id = MAKEID('S', 'C', 'H', 'D');
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL SCSIHD::Reset()
{
	// Unlock and release attention
	disk.lock = FALSE;
	disk.attn = FALSE;

	// No reset, clear code
	disk.reset = FALSE;
	disk.code = 0x00;
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIHD::Open(const Filepath& path, BOOL /*attn*/)
{
	Fileio fio;
	off64_t size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// read open required
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();
	fio.Close();

	// Must be 512 bytes
	if (size & 0x1ff) {
		return FALSE;
	}

	// 10MB or more
	if (size < 0x9f5400) {
		return FALSE;
	}
    // 2TB according to xm6i
    // There is a similar one in wxw/wxw_cfg.cpp
	if (size > 2LL * 1024 * 1024 * 1024 * 1024) {
		return FALSE;
	}

	// sector size and number of blocks
	disk.size = 9;
	disk.blocks = (DWORD)(size >> 9);

	// Call base class
	return Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	char vendor[32];
	char product[32];
	char rev[32];
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return 0;
	}

	// Ready check (Error if no image file)
	if (!disk.ready) {
		disk.code = DISK_NOTREADY;
		return 0;
	}

	// Basic data
	// buf[0] ... Direct Access Device
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buf[0] = 0x7f;
	}

	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 122 + 3;	// Value close to real HDD

	// Fill with blanks
	memset(&buf[8], 0x20, buf[4] - 3);

	// Determine vendor name/product name
	sprintf(vendor, BENDER_SIGNATURE);
	size = disk.blocks >> 11;
	if (size < 300)
		sprintf(product, "PRODRIVE LPS%dS", size);
	else if (size < 600)
		sprintf(product, "MAVERICK%dS", size);
	else if (size < 800)
		sprintf(product, "LIGHTNING%dS", size);
	else if (size < 1000)
		sprintf(product, "TRAILBRAZER%dS", size);
	else if (size < 2000)
		sprintf(product, "FIREBALL%dS", size);
	else
		sprintf(product, "FBSE%d.%dS", size / 1000, (size % 1000) / 100);

	// Vendor name
	memcpy(&buf[8], vendor, strlen(vendor));

	// Product name
	memcpy(&buf[16], product, strlen(product));

	// Revision
	sprintf(rev, "0%01d%01d%01d",
			(int)major, (int)(minor >> 4), (int)(minor & 0x0f));
	memcpy(&buf[32], rev, 4);

	// Size of data that can be returned
	size = (buf[4] + 5);

	// Limit if the other buffer is small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	//  Success
	disk.code = DISK_NOERROR;
	return size;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//	*Not affected by disk.code
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIHD::ModeSelect(const DWORD *cdb, const BYTE *buf, int length)
{
	int page;
	int size;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(length >= 0);

	// PF
	if (cdb[1] & 0x10) {
		// Mode Parameter header
		if (length >= 12) {
			// Check the block length bytes
			size = 1 << disk.size;
			if (buf[9] != (BYTE)(size >> 16) ||
				buf[10] != (BYTE)(size >> 8) ||
				buf[11] != (BYTE)size) {
				// currently does not allow changing sector length
				disk.code = DISK_INVALIDPRM;
				return FALSE;
			}
			buf += 12;
			length -= 12;
		}

		// Parsing the page
		while (length > 0) {
			// Get page
			page = buf[0];

			switch (page) {
				// format device
				case 0x03:
					// check the number of bytes in the physical sector
					size = 1 << disk.size;
					if (buf[0xc] != (BYTE)(size >> 8) ||
						buf[0xd] != (BYTE)size) {
						// currently does not allow changing sector length
						disk.code = DISK_INVALIDPRM;
						return FALSE;
					}
					break;

				// Other page
				default:
					break;
			}

			// Advance to the next page
			size = buf[1] + 2;
			length -= size;
			buf += size;
		}
	}

	// Do not generate an error for the time being (MINIX)
	disk.code = DISK_NOERROR;

	return TRUE;
}

//===========================================================================
//
//	SCSI hard disk (PC-9801-55 NEC genuine /Anex86/T98Next)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIHD_NEC::SCSIHD_NEC() : SCSIHD()
{
	// ワーク初期化
	cylinders = 0;
	heads = 0;
	sectors = 0;
	sectorsize = 0;
	imgoffset = 0;
	imgsize = 0;
}

//---------------------------------------------------------------------------
//
//	リトルエンディアンと想定したワードを取り出す
//
//---------------------------------------------------------------------------
static inline WORD getWordLE(const BYTE *b)
{
	return ((WORD)(b[1]) << 8) | b[0];
}

//---------------------------------------------------------------------------
//
//	リトルエンディアンと想定したロングワードを取り出す
//
//---------------------------------------------------------------------------
static inline DWORD getDwordLE(const BYTE *b)
{
	return ((DWORD)(b[3]) << 24) | ((DWORD)(b[2]) << 16) |
		((DWORD)(b[1]) << 8) | b[0];
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIHD_NEC::Open(const Filepath& path, BOOL /*attn*/)
{
	Fileio fio;
	off64_t size;
	BYTE hdr[512];
	LPCTSTR ext;

	ASSERT(this);
	ASSERT(!disk.ready);

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();

	// ヘッダー読み込み
	if (size >= (off64_t)sizeof(hdr)) {
		if (!fio.Read(hdr, sizeof(hdr))) {
			fio.Close();
			return FALSE;
		}
	}
	fio.Close();

	// 512バイト単位であること
	if (size & 0x1ff) {
		return FALSE;
	}

	// 10MB以上
	if (size < 0x9f5400) {
		return FALSE;
	}
	// xm6iに準じて2TB
	// よく似たものが wxw/wxw_cfg.cpp にもある
	if (size > 2LL * 1024 * 1024 * 1024 * 1024) {
		return FALSE;
	}

	// 拡張子別にパラメータを決定
	ext = path.GetFileExt();
	if (xstrcasecmp(ext, _T(".HDN")) == 0) {
		// デフォルト設定としてセクタサイズ512,セクタ数25,ヘッド数8を想定
		imgoffset = 0;
		imgsize = size;
		sectorsize = 512;
		sectors = 25;
		heads = 8;
		cylinders = (int)(size >> 9);
		cylinders >>= 3;
		cylinders /= 25;
	} else if (xstrcasecmp(ext, _T(".HDI")) == 0) { // Anex86 HD image?
		imgoffset = getDwordLE(&hdr[4 + 4]);
		imgsize = getDwordLE(&hdr[4 + 4 + 4]);
		sectorsize = getDwordLE(&hdr[4 + 4 + 4 + 4]);
		sectors = getDwordLE(&hdr[4 + 4 + 4 + 4 + 4]);
		heads = getDwordLE(&hdr[4 + 4 + 4 + 4 + 4 + 4]);
		cylinders = getDwordLE(&hdr[4 + 4 + 4 + 4 + 4 + 4 + 4]);
	} else if (xstrcasecmp(ext, _T(".NHD")) == 0 &&
		memcmp(hdr, "T98HDDIMAGE.R0\0", 15) == 0) { // T98Next HD image?
		imgoffset = getDwordLE(&hdr[0x10 + 0x100]);
		cylinders = getDwordLE(&hdr[0x10 + 0x100 + 4]);
		heads = getWordLE(&hdr[0x10 + 0x100 + 4 + 4]);
		sectors = getWordLE(&hdr[0x10 + 0x100 + 4 + 4 + 2]);
		sectorsize = getWordLE(&hdr[0x10 + 0x100 + 4 + 4 + 2 + 2]);
		imgsize = (off64_t)cylinders * heads * sectors * sectorsize;
	}

	// セクタサイズは256または512をサポート
	if (sectorsize != 256 && sectorsize != 512) {
		return FALSE;
	}

	// イメージサイズの整合性チェック
	if (imgoffset + imgsize > size || (imgsize % sectorsize != 0)) {
		return FALSE;
	}

	// セクタサイズ
	for(disk.size = 16; disk.size > 0; --(disk.size)) {
		if ((1 << disk.size) == sectorsize)
			break;
	}
	if (disk.size <= 0 || disk.size > 16) {
		return FALSE;
	}

	// ブロック数
	disk.blocks = (DWORD)(imgsize >> disk.size);
	disk.imgoffset = imgoffset;

	// Call the base class
	return Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_NEC::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	int size;

	// 基底クラス
	size = SCSIHD::Inquiry(cdb, buf, major, minor);

	// 基底クラスでエラーなら終了
	if (size == 0) {
		return 0;
	}

	// SCSI1相当に変更
	buf[2] = 0x01;
	buf[3] = 0x01;

	// Replace Vendor name
	buf[8] = 'N';
	buf[9] = 'E';
	buf[10] = 'C';

	return size;
}

//---------------------------------------------------------------------------
//
//	エラーページ追加
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_NEC::AddError(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x01;
	buf[1] = 0x06;

	// No changeable area
	if (change) {
		return 8;
	}

	// リトライカウントは0、リミットタイムは装置内部のデフォルト値を使用
	return 8;
}

//---------------------------------------------------------------------------
//
//	フォーマットページ追加
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_NEC::AddFormat(BOOL change, BYTE *buf)
{
	int size;

	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x80 | 0x03;
	buf[1] = 0x16;

	// 物理セクタのバイト数は変更可能に見せる(実際には変更できないが)
	if (change) {
		buf[0xc] = 0xff;
		buf[0xd] = 0xff;
		return 24;
	}

	if (disk.ready) {
		// 1ゾーンのトラック数を設定(PC-9801-55はこの値を見ているようだ)
		buf[0x2] = (BYTE)(heads >> 8);
		buf[0x3] = (BYTE)heads;

		// 1トラックのセクタ数を設定
		buf[0xa] = (BYTE)(sectors >> 8);
		buf[0xb] = (BYTE)sectors;

		// 物理セクタのバイト数を設定
		size = 1 << disk.size;
		buf[0xc] = (BYTE)(size >> 8);
		buf[0xd] = (BYTE)size;
	}

	// リムーバブル属性を設定(昔の名残)
	if (disk.removable) {
		buf[20] = 0x20;
	}

	return 24;
}

//---------------------------------------------------------------------------
//
//	ドライブページ追加
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_NEC::AddDrive(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set the message length
	buf[0] = 0x04;
	buf[1] = 0x12;

	// No changeable area
	if (change) {
		return 20;
	}

	if (disk.ready) {
		// シリンダ数を設定
		buf[0x2] = (BYTE)(cylinders >> 16);
		buf[0x3] = (BYTE)(cylinders >> 8);
		buf[0x4] = (BYTE)cylinders;

		// ヘッド数を設定
		buf[0x5] = (BYTE)heads;
	}

	return 20;
}

//===========================================================================
//
//	SCSI hard disk (Macintosh Apple genuine)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIHD_APPLE::SCSIHD_APPLE() : SCSIHD()
{
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_APPLE::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	int size;
	char vendor[32];
	char product[32];

	// Call the base class
	size = SCSIHD::Inquiry(cdb, buf, major, minor);

	// End if there is an error in the base class
	if (size == 0) {
		return 0;
	}

	// Vendor name
	sprintf(vendor, " SEAGATE");
	memcpy(&buf[8], vendor, strlen(vendor));

	// Product name
	sprintf(product, "          ST225N");
	memcpy(&buf[16], product, strlen(product));

	return size;
}

//---------------------------------------------------------------------------
//
//	Add Vendor special page
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_APPLE::AddVendor(int page, BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Page code 48
	if ((page != 0x30) && (page != 0x3f)) {
		return 0;
	}

	// Set the message length
	buf[0] = 0x30;
	buf[1] = 0x1c;

	// No changeable area
	if (change) {
		return 30;
	}

	// APPLE COMPUTER, INC.
	memcpy(&buf[0xa], "APPLE COMPUTER, INC.", 20);

	return 30;
}

//===========================================================================
//
//	SCSI magneto-optical disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIMO::SCSIMO() : Disk()
{
	// SCSI magneto-optical disk
	disk.id = MAKEID('S', 'C', 'M', 'O');

	// Set as removable
	disk.removable = TRUE;
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIMO::Open(const Filepath& path, BOOL attn)
{
	Fileio fio;
	off64_t size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();
	fio.Close();

	switch (size) {
		// 128MB
		case 0x797f400:
			disk.size = 9;
			disk.blocks = 248826;
			break;

		// 230MB
		case 0xd9eea00:
			disk.size = 9;
			disk.blocks = 446325;
			break;

		// 540MB
		case 0x1fc8b800:
			disk.size = 9;
			disk.blocks = 1041500;
			break;

		// 640MB
		case 0x25e28000:
			disk.size = 11;
			disk.blocks = 310352;
			break;

		// Other (this is an error)
		default:
			return FALSE;
	}

	// Call the base class
	Disk::Open(path);

	// Attention if ready
	if (disk.ready && attn) {
		disk.attn = TRUE;
	}

	return TRUE;
}

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIMO::Load(Fileio *fio, int ver)
{
	DWORD sz;
	disk_t buf;
	DWORD padding;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);

	// Prior to version 2.03, the disk was not saved
	if (ver <= 0x0202) {
		return TRUE;
	}

	// load size, match
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != 52) {
		return FALSE;
	}

	// load into buffer
	PROP_IMPORT(fio, buf.id);
	PROP_IMPORT(fio, buf.ready);
	PROP_IMPORT(fio, buf.writep);
	PROP_IMPORT(fio, buf.readonly);
	PROP_IMPORT(fio, buf.removable);
	PROP_IMPORT(fio, buf.lock);
	PROP_IMPORT(fio, buf.attn);
	PROP_IMPORT(fio, buf.reset);
	PROP_IMPORT(fio, buf.size);
	PROP_IMPORT(fio, buf.blocks);
	PROP_IMPORT(fio, buf.lun);
	PROP_IMPORT(fio, buf.code);
	PROP_IMPORT(fio, padding);

	// Load path
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// Always eject
	Eject(TRUE);

	// Move only if IDs match
	if (disk.id != buf.id) {
		// Not MO at the time of save. Maintain eject status
		return TRUE;
	}

	// Re-try opening
	if (!Open(path, FALSE)) {
		// Cannot reopen. Maintain eject status
		return TRUE;
	}

	// Disk cache is created in Open. Move property only
	if (!disk.readonly) {
		disk.writep = buf.writep;
	}
	disk.lock = buf.lock;
	disk.attn = buf.attn;
	disk.reset = buf.reset;
	disk.lun = buf.lun;
	disk.code = buf.code;

	// loaded successfully
	return TRUE;
}
#endif	// RASCSI

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIMO::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	int size;
	char rev[32];

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// 基本データ
	// buf[0] ... Optical Memory Device
	// buf[1] ... Removable
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[0] = 0x07;

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buf[0] = 0x7f;
	}

	buf[1] = 0x80;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 36 - 5;	// required

	// Fill with blanks
	memset(&buf[8], 0x20, buf[4] - 3);

	// Vendor name
	memcpy(&buf[8], BENDER_SIGNATURE, strlen(BENDER_SIGNATURE));

	// Product name
	memcpy(&buf[16], "M2513A", 6);

	// Revision (XM6 version number)
	sprintf(rev, "0%01d%01d%01d",
				(int)major, (int)(minor >> 4), (int)(minor & 0x0f));
	memcpy(&buf[32], rev, 4);

	// Size return data
	size = (buf[4] + 5);

	// Limit the size if the buffer is too small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	//  Success
	disk.code = DISK_NOERROR;
	return size;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//	*Not affected by disk.code
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIMO::ModeSelect(const DWORD *cdb, const BYTE *buf, int length)
{
	int page;
	int size;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(length >= 0);

	// PF
	if (cdb[1] & 0x10) {
		// Mode Parameter header
		if (length >= 12) {
			// Check the block length (in bytes)
			size = 1 << disk.size;
			if (buf[9] != (BYTE)(size >> 16) ||
				buf[10] != (BYTE)(size >> 8) || buf[11] != (BYTE)size) {
				// Currently does not allow changing sector length
				disk.code = DISK_INVALIDPRM;
				return FALSE;
			}
			buf += 12;
			length -= 12;
		}

		// Parsing the page
		while (length > 0) {
			// Get the page
			page = buf[0];

			switch (page) {
				// format device
				case 0x03:
					// Check the number of bytes in the physical sector
					size = 1 << disk.size;
					if (buf[0xc] != (BYTE)(size >> 8) ||
						buf[0xd] != (BYTE)size) {
						// Currently does not allow changing sector length
						disk.code = DISK_INVALIDPRM;
						return FALSE;
					}
					break;
				// vendor unique format
				case 0x20:
					// just ignore, for now
					break;

				// Other page
				default:
					break;
			}

			// Advance to the next page
			size = buf[1] + 2;
			length -= size;
			buf += size;
		}
	}

	// Do not generate an error for the time being (MINIX)
	disk.code = DISK_NOERROR;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Vendor Unique Format Page 20h (MO)
//
//---------------------------------------------------------------------------
int FASTCALL SCSIMO::AddVendor(int page, BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Page code 20h
	if ((page != 0x20) && (page != 0x3f)) {
		return 0;
	}

	// Set the message length
	buf[0] = 0x20;
	buf[1] = 0x0a;

	// No changeable area
	if (change) {
		return 12;
	}

	/*
		mode page code 20h - Vendor Unique Format Page
		format mode XXh type 0
		information: http://h20628.www2.hp.com/km-ext/kmcsdirect/emr_na-lpg28560-1.pdf

		offset  description
		  02h   format mode
		  03h   type of format (0)
		04~07h  size of user band (total sectors?)
		08~09h  size of spare band (spare sectors?)
		0A~0Bh  number of bands

		actual value of each 3.5inches optical medium (grabbed by Fujitsu M2513EL)

		                     128M     230M    540M    640M
		---------------------------------------------------
		size of user band   3CBFAh   6CF75h  FE45Ch  4BC50h
		size of spare band   0400h    0401h   08CAh   08C4h
		number of bands      0001h    000Ah   0012h   000Bh

		further information: http://r2089.blog36.fc2.com/blog-entry-177.html
	*/

	if (disk.ready) {
		unsigned spare = 0;
		unsigned bands = 0;

		if (disk.size == 9) switch (disk.blocks) {
			// 128MB
			case 248826:
				spare = 1024;
				bands = 1;
				break;

			// 230MB
			case 446325:
				spare = 1025;
				bands = 10;
				break;

			// 540MB
			case 1041500:
				spare = 2250;
				bands = 18;
				break;
		}

		if (disk.size == 11) switch (disk.blocks) {
			// 640MB
			case 310352:
				spare = 2244;
				bands = 11;
				break;

			// 1.3GB (lpproj: not tested with real device)
			case 605846:
				spare = 4437;
				bands = 18;
				break;
		}

		buf[2] = 0; // format mode
		buf[3] = 0; // type of format
		buf[4] = (BYTE)(disk.blocks >> 24);
		buf[5] = (BYTE)(disk.blocks >> 16);
		buf[6] = (BYTE)(disk.blocks >> 8);
		buf[7] = (BYTE)disk.blocks;
		buf[8] = (BYTE)(spare >> 8);
		buf[9] = (BYTE)spare;
		buf[10] = (BYTE)(bands >> 8);
		buf[11] = (BYTE)bands;
	}

	return 12;
}

//===========================================================================
//
//	CD Track
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CDTrack::CDTrack(SCSICD *scsicd)
{
	ASSERT(scsicd);

	// Set parent CD-ROM device
	cdrom = scsicd;

	// Track defaults to disabled
	valid = FALSE;

	// Initialize other data
	track_no = -1;
	first_lba = 0;
	last_lba = 0;
	audio = FALSE;
	raw = FALSE;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
CDTrack::~CDTrack()
{
}

//---------------------------------------------------------------------------
//
//	Init
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::Init(int track, DWORD first, DWORD last)
{
	ASSERT(this);
	ASSERT(!valid);
	ASSERT(track >= 1);
	ASSERT(first < last);

	// Set and enable track number
	track_no = track;
	valid = TRUE;

	// Remember LBA
	first_lba = first;
	last_lba = last;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Set Path
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::SetPath(BOOL cdda, const Filepath& path)
{
	ASSERT(this);
	ASSERT(valid);

	// CD-DA or data
	audio = cdda;

	// Remember the path
	imgpath = path;
}

//---------------------------------------------------------------------------
//
//	Get Path
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::GetPath(Filepath& path) const
{
	ASSERT(this);
	ASSERT(valid);

	// Return the path (by reference)
	path = imgpath;
}

//---------------------------------------------------------------------------
//
//	Add Index
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::AddIndex(int index, DWORD lba)
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(index > 0);
	ASSERT(first_lba <= lba);
	ASSERT(lba <= last_lba);

	// Currently does not support indexes
	ASSERT(FALSE);
}

//---------------------------------------------------------------------------
//
//	Gets the start of LBA
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetFirst() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	return first_lba;
}

//---------------------------------------------------------------------------
//
//	Get the end of LBA
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetLast() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	return last_lba;
}

//---------------------------------------------------------------------------
//
//	Get the number of blocks
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetBlocks() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	// Calculate from start LBA and end LBA
	return (DWORD)(last_lba - first_lba + 1);
}

//---------------------------------------------------------------------------
//
//	Get track number
//
//---------------------------------------------------------------------------
int FASTCALL CDTrack::GetTrackNo() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(track_no >= 1);

	return track_no;
}

//---------------------------------------------------------------------------
//
//	Is valid block
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::IsValid(DWORD lba) const
{
	ASSERT(this);

	// FALSE if the track itself is invalid
	if (!valid) {
		return FALSE;
	}

	// If the block is BEFORE the first block
	if (lba < first_lba) {
		return FALSE;
	}

	// If the block is AFTER the last block
	if (last_lba < lba) {
		return FALSE;
	}

	// This track is valid
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Is audio track
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::IsAudio() const
{
	ASSERT(this);
	ASSERT(valid);

	return audio;
}

//===========================================================================
//
//	CD-DA Buffer
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CDDABuf::CDDABuf()
{
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
CDDABuf::~CDDABuf()
{
}

//===========================================================================
//
//	SCSI CD-ROM
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSICD::SCSICD() : Disk()
{
	int i;

	// SCSI CD-ROM
	disk.id = MAKEID('S', 'C', 'C', 'D');

	// removable, write protected
	disk.removable = TRUE;
	disk.writep = TRUE;

	// NOT in raw format
	rawfile = FALSE;

	// Frame initialization
	frame = 0;

	// Track initialization
	for (i = 0; i < TrackMax; i++) {
		track[i] = NULL;
	}
	tracks = 0;
	dataindex = -1;
	audioindex = -1;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SCSICD::~SCSICD()
{
	// Clear track
	ClearTrack();
}

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::Load(Fileio *fio, int ver)
{
	DWORD sz;
	disk_t buf;
	DWORD padding;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);

	// Prior to version 2.03, the disk was not saved
	if (ver <= 0x0202) {
		return TRUE;
	}

	// load size, match
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != 52) {
		return FALSE;
	}

	// load into buffer
	PROP_IMPORT(fio, buf.id);
	PROP_IMPORT(fio, buf.ready);
	PROP_IMPORT(fio, buf.writep);
	PROP_IMPORT(fio, buf.readonly);
	PROP_IMPORT(fio, buf.removable);
	PROP_IMPORT(fio, buf.lock);
	PROP_IMPORT(fio, buf.attn);
	PROP_IMPORT(fio, buf.reset);
	PROP_IMPORT(fio, buf.size);
	PROP_IMPORT(fio, buf.blocks);
	PROP_IMPORT(fio, buf.lun);
	PROP_IMPORT(fio, buf.code);
	PROP_IMPORT(fio, padding);

	// Load path
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// Always eject
	Eject(TRUE);

	// move only if IDs match
	if (disk.id != buf.id) {
		// It was not a CD-ROM when saving. Maintain eject status
		return TRUE;
	}

	// Try to reopen
	if (!Open(path, FALSE)) {
		// Cannot reopen. Maintain eject status
		return TRUE;
	}

	// Disk cache is created in Open. Move property only
	if (!disk.readonly) {
		disk.writep = buf.writep;
	}
	disk.lock = buf.lock;
	disk.attn = buf.attn;
	disk.reset = buf.reset;
	disk.lun = buf.lun;
	disk.code = buf.code;

	// Discard the disk cache again
	if (disk.dcache) {
		delete disk.dcache;
		disk.dcache = NULL;
	}
	disk.dcache = NULL;

	// Tentative
	disk.blocks = track[0]->GetBlocks();
	if (disk.blocks > 0) {
		// Recreate the disk cache
		track[0]->GetPath(path);
		disk.dcache = new DiskCache(path, disk.size, disk.blocks);
		disk.dcache->SetRawMode(rawfile);

		// Reset data index
		dataindex = 0;
	}

	return TRUE;
}
#endif	// RASCSI

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::Open(const Filepath& path, BOOL attn)
{
	Fileio fio;
	off64_t size;
	TCHAR file[5];

	ASSERT(this);
	ASSERT(!disk.ready);

	// Initialization, track clear
	disk.blocks = 0;
	rawfile = FALSE;
	ClearTrack();

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Close and transfer for physical CD access
	if (path.GetPath()[0] == _T('\\')) {
		// Close
		fio.Close();

		// Open physical CD
		if (!OpenPhysical(path)) {
			return FALSE;
		}
	} else {
		// Get file size
        size = fio.GetFileSize();
		if (size <= 4) {
			fio.Close();
			return FALSE;
		}

		// Judge whether it is a CUE sheet or an ISO file
		fio.Read(file, 4);
		file[4] = '\0';
		fio.Close();

		// If it starts with FILE, consider it as a CUE sheet
		if (xstrncasecmp(file, _T("FILE"), 4) == 0) {
			// Open as CUE
			if (!OpenCue(path)) {
				return FALSE;
			}
		} else {
			// Open as ISO
			if (!OpenIso(path)) {
				return FALSE;
			}
		}
	}

	// Successful opening
	ASSERT(disk.blocks > 0);
	disk.size = 11;

	// Call the base class
	Disk::Open(path);

	// Set RAW flag
	ASSERT(disk.dcache);
	disk.dcache->SetRawMode(rawfile);

	// Since it is a ROM media, writing is not possible
	disk.writep = TRUE;

	// Attention if ready
	if (disk.ready && attn) {
		disk.attn = TRUE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Open (CUE)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::OpenCue(const Filepath& /*path*/)
{
	ASSERT(this);

	// Always fail
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	オープン(ISO)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::OpenIso(const Filepath& path)
{
	Fileio fio;
	off64_t size;
	BYTE header[12];
	BYTE sync[12];

	ASSERT(this);

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();
	if (size < 0x800) {
		fio.Close();
		return FALSE;
	}

	// Read the first 12 bytes and close
	if (!fio.Read(header, sizeof(header))) {
		fio.Close();
		return FALSE;
	}

	// Check if it is RAW format
	memset(sync, 0xff, sizeof(sync));
	sync[0] = 0x00;
	sync[11] = 0x00;
	rawfile = FALSE;
	if (memcmp(header, sync, sizeof(sync)) == 0) {
		// 00,FFx10,00, so it is presumed to be RAW format
		if (!fio.Read(header, 4)) {
			fio.Close();
			return FALSE;
		}

		// Supports MODE1/2048 or MODE1/2352 only
		if (header[3] != 0x01) {
			// Different mode
			fio.Close();
			return FALSE;
		}

		// Set to RAW file
		rawfile = TRUE;
	}
	fio.Close();

	if (rawfile) {
		// Size must be a multiple of 2536 and less than 700MB
		if (size % 0x930) {
			return FALSE;
		}
		if (size > 912579600) {
			return FALSE;
		}

		// Set the number of blocks
		disk.blocks = (DWORD)(size / 0x930);
	} else {
		// Size must be a multiple of 2048 and less than 700MB
		if (size & 0x7ff) {
			return FALSE;
		}
		if (size > 0x2bed5000) {
			return FALSE;
		}

		// Set the number of blocks
		disk.blocks = (DWORD)(size >> 11);
	}

	// Create only one data track
	ASSERT(!track[0]);
	track[0] = new CDTrack(this);
	track[0]->Init(1, 0, disk.blocks - 1);
	track[0]->SetPath(FALSE, path);
	tracks = 1;
	dataindex = 0;

	// Successful opening
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Open (Physical)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::OpenPhysical(const Filepath& path)
{
	Fileio fio;
	off64_t size;

	ASSERT(this);

	// Open as read-only
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get size
	size = fio.GetFileSize();
	if (size < 0x800) {
		fio.Close();
		return FALSE;
	}

	// Close
	fio.Close();

	// Size must be a multiple of 2048 and less than 700MB
	if (size & 0x7ff) {
		return FALSE;
	}
	if (size > 0x2bed5000) {
		return FALSE;
	}

	// Set the number of blocks
	disk.blocks = (DWORD)(size >> 11);

	// Create only one data track
	ASSERT(!track[0]);
	track[0] = new CDTrack(this);
	track[0]->Init(1, 0, disk.blocks - 1);
	track[0]->SetPath(FALSE, path);
	tracks = 1;
	dataindex = 0;

	// Successful opening
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	char rev[32];
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Basic data
	// buf[0] ... CD-ROM Device
	// buf[1] ... Removable
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[0] = 0x05;

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buf[0] = 0x7f;
	}

	buf[1] = 0x80;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 42;	// Required

	// Fill with blanks
	memset(&buf[8], 0x20, buf[4] - 3);

	// Vendor name
	memcpy(&buf[8], BENDER_SIGNATURE, strlen(BENDER_SIGNATURE));

	// Product name
	memcpy(&buf[16], "CD-ROM CDU-8003A", 16);

	// Revision (XM6 version number)
//	sprintf(rev, "1.9a",
	//			(int)major, (int)(minor >> 4), (int)(minor & 0x0f));
	memcpy(&buf[32], "1.9a", 4);

	//strcpy(&buf[35],"A1.9a");
	buf[36]=0x20;
	memcpy(&buf[37],"1999/01/01",10);

	// Size of data that can be returned
	size = (buf[4] + 5);

	// Limit if the other buffer is small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	//  Success
	disk.code = DISK_NOERROR;
	return size;
}

//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::Read(BYTE *buf, DWORD block)
{
	int index;
	Filepath path;

	ASSERT(this);
	ASSERT(buf);

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Search for the track
	index = SearchTrack(block);

	// if invalid, out of range
	if (index < 0) {
		disk.code = DISK_INVALIDLBA;
		return 0;
	}
	ASSERT(track[index]);

	// If different from the current data track
	if (dataindex != index) {
		// Delete current disk cache (no need to save)
		delete disk.dcache;
		disk.dcache = NULL;

		// Reset the number of blocks
		disk.blocks = track[index]->GetBlocks();
		ASSERT(disk.blocks > 0);

		// Recreate the disk cache
		track[index]->GetPath(path);
		disk.dcache = new DiskCache(path, disk.size, disk.blocks);
		disk.dcache->SetRawMode(rawfile);

		// Reset data index
		dataindex = index;
	}

	// Base class
	ASSERT(dataindex >= 0);
	return Disk::Read(buf, block);
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::ReadToc(const DWORD *cdb, BYTE *buf)
{
	int last;
	int index;
	int length;
	int loop;
	int i;
	BOOL msf;
	DWORD lba;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x43);
	ASSERT(buf);

	// Check if ready
	if (!CheckReady()) {
		return 0;
	}

	// If ready, there is at least one track
	ASSERT(tracks > 0);
	ASSERT(track[0]);

	// Get allocation length, clear buffer
	length = cdb[7] << 8;
	length |= cdb[8];
	memset(buf, 0, length);

	// Get MSF Flag
	if (cdb[1] & 0x02) {
		msf = TRUE;
	} else {
		msf = FALSE;
	}

	// Get and check the last track number
	last = track[tracks - 1]->GetTrackNo();
	if ((int)cdb[6] > last) {
		// Except for AA
		if (cdb[6] != 0xaa) {
			disk.code = DISK_INVALIDCDB;
			return 0;
		}
	}

	// Check start index
	index = 0;
	if (cdb[6] != 0x00) {
		// Advance the track until the track numbers match
		while (track[index]) {
			if ((int)cdb[6] == track[index]->GetTrackNo()) {
				break;
			}
			index++;
		}

		// AA if not found or internal error
		if (!track[index]) {
			if (cdb[6] == 0xaa) {
				// Returns the final LBA+1 because it is AA
				buf[0] = 0x00;
				buf[1] = 0x0a;
				buf[2] = (BYTE)track[0]->GetTrackNo();
				buf[3] = (BYTE)last;
				buf[6] = 0xaa;
				lba = track[tracks - 1]->GetLast() + 1;
				if (msf) {
					LBAtoMSF(lba, &buf[8]);
				} else {
					buf[10] = (BYTE)(lba >> 8);
					buf[11] = (BYTE)lba;
				}
				return length;
			}

			// Otherwise, error
			disk.code = DISK_INVALIDCDB;
			return 0;
		}
	}

	// Number of track descriptors returned this time (number of loops)
	loop = last - track[index]->GetTrackNo() + 1;
	ASSERT(loop >= 1);

	// Create header
	buf[0] = (BYTE)(((loop << 3) + 2) >> 8);
	buf[1] = (BYTE)((loop << 3) + 2);
	buf[2] = (BYTE)track[0]->GetTrackNo();
	buf[3] = (BYTE)last;
	buf += 4;

	// Loop....
	for (i = 0; i < loop; i++) {
		// ADR and Control
		if (track[index]->IsAudio()) {
			// audio track
			buf[1] = 0x10;
		} else {
			// data track
			buf[1] = 0x14;
		}

		// track number
		buf[2] = (BYTE)track[index]->GetTrackNo();

		// track address
		if (msf) {
			LBAtoMSF(track[index]->GetFirst(), &buf[4]);
		} else {
			buf[6] = (BYTE)(track[index]->GetFirst() >> 8);
			buf[7] = (BYTE)(track[index]->GetFirst());
		}

		// Advance buffer pointer and index
		buf += 8;
		index++;
	}

	// Always return only the allocation length
	return length;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudio(const DWORD* /*cdb*/)
{
	ASSERT(this);

	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO MSF
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudioMSF(const DWORD* /*cdb*/)
{
	ASSERT(this);

	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO TRACK
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudioTrack(const DWORD* /*cdb*/)
{
	ASSERT(this);

	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	LBA→MSF Conversion
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::LBAtoMSF(DWORD lba, BYTE *msf) const
{
	DWORD m;
	DWORD s;
	DWORD f;

	ASSERT(this);

	// 75 and 75*60 get the remainder
	m = lba / (75 * 60);
	s = lba % (75 * 60);
	f = s % 75;
	s /= 75;

	// The base point is M=0, S=2, F=0
	s += 2;
	if (s >= 60) {
		s -= 60;
		m++;
	}

	// Store
	ASSERT(m < 0x100);
	ASSERT(s < 60);
	ASSERT(f < 75);
	msf[0] = 0x00;
	msf[1] = (BYTE)m;
	msf[2] = (BYTE)s;
	msf[3] = (BYTE)f;
}

//---------------------------------------------------------------------------
//
//	MSF→LBA Conversion
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSICD::MSFtoLBA(const BYTE *msf) const
{
	DWORD lba;

	ASSERT(this);
	ASSERT(msf[2] < 60);
	ASSERT(msf[3] < 75);

	// 1, 75, add up in multiples of 75*60
	lba = msf[1];
	lba *= 60;
	lba += msf[2];
	lba *= 75;
	lba += msf[3];

	// Since the base point is M=0, S=2, F=0, subtract 150
	lba -= 150;

	return lba;
}

//---------------------------------------------------------------------------
//
//	Clear Track
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::ClearTrack()
{
	int i;

	ASSERT(this);

	// delete the track object
	for (i = 0; i < TrackMax; i++) {
		if (track[i]) {
			delete track[i];
			track[i] = NULL;
		}
	}

	// Number of tracks is 0
	tracks = 0;

	// No settings for data and audio
	dataindex = -1;
	audioindex = -1;
}

//---------------------------------------------------------------------------
//
//	Track Search
//	* Returns -1 if not found
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::SearchTrack(DWORD lba) const
{
	int i;

	ASSERT(this);

	// Track loop
	for (i = 0; i < tracks; i++) {
		// Listen to the track
		ASSERT(track[i]);
		if (track[i]->IsValid(lba)) {
			return i;
		}
	}

	// Track wasn't found
	return -1;
}

//---------------------------------------------------------------------------
//
//	Next Frame
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::NextFrame()
{
	ASSERT(this);
	ASSERT((frame >= 0) && (frame < 75));

	// set the frame in the range 0-74
	frame = (frame + 1) % 75;

	// FALSE after one lap
	if (frame != 0) {
		return TRUE;
	} else {
		return FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Get CD-DA buffer
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::GetBuf(
	DWORD* /*buffer*/, int /*samples*/, DWORD /*rate*/)
{
	ASSERT(this);
}


//===========================================================================
//
//	SCSI Host Bridge
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIBR::SCSIBR() : Disk()
{
	// Host Bridge
	disk.id = MAKEID('S', 'C', 'B', 'R');

#if defined(RASCSI) && !defined(BAREMETAL)
	// TAP Driver Generation
	tap = new CTapDriver();
	m_bTapEnable = tap->Init();

	// Generate MAC Address
	memset(mac_addr, 0x00, 6);
	if (m_bTapEnable) {
		tap->GetMacAddr(mac_addr);
		mac_addr[5]++;
	}

	// Packet reception flag OFF
	packet_enable = FALSE;
#endif	// RASCSI && !BAREMETAL

	// Create host file system
	fs = new CFileSys();
	fs->Reset();
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SCSIBR::~SCSIBR()
{
#if defined(RASCSI) && !defined(BAREMETAL)
	// TAP driver release
	if (tap) {
		tap->Cleanup();
		delete tap;
	}
#endif	// RASCSI && !BAREMETAL

	// Release host file system
	if (fs) {
		fs->Reset();
		delete fs;
	}
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	char rev[32];
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Basic data
	// buf[0] ... Communication Device
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[0] = 0x09;

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buf[0] = 0x7f;
	}

	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 36 - 5 + 8;	// required + 8 byte extension

	// Fill with blanks
	memset(&buf[8], 0x20, buf[4] - 3);

	// Vendor name
	memcpy(&buf[8], BENDER_SIGNATURE, strlen(BENDER_SIGNATURE));

	// Product name
	memcpy(&buf[16], "RASCSI BRIDGE", 13);

	// Revision (XM6 version number)
	sprintf(rev, "0%01d%01d%01d",
				(int)major, (int)(minor >> 4), (int)(minor & 0x0f));
	memcpy(&buf[32], rev, 4);

	// Optional function valid flag
	buf[36] = '0';

#if defined(RASCSI) && !defined(BAREMETAL)
	// TAP Enable
	if (m_bTapEnable) {
		buf[37] = '1';
	}
#endif	// RASCSI && !BAREMETAL

	// CFileSys Enable
	buf[38] = '1';

	// Size of data that can be returned
	size = (buf[4] + 5);

	// Limit if the other buffer is small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	//  Success
	disk.code = DISK_NOERROR;
	return size;
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIBR::TestUnitReady(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// TEST UNIT READY Success
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	GET MESSAGE(10)
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::GetMessage10(const DWORD *cdb, BYTE *buf)
{
	int type;
	int phase;
#if defined(RASCSI) && !defined(BAREMETAL)
	int func;
	int total_len;
	int i;
#endif	// RASCSI && !BAREMETAL

	ASSERT(this);

	// Type
	type = cdb[2];

#if defined(RASCSI) && !defined(BAREMETAL)
	// Function number
	func = cdb[3];
#endif	// RASCSI && !BAREMETAL

	// Phase
	phase = cdb[9];

	switch (type) {
#if defined(RASCSI) && !defined(BAREMETAL)
		case 1:		// Ethernet
			// Do not process if TAP is invalid
			if (!m_bTapEnable) {
				return 0;
			}

			switch (func) {
				case 0:		// Get MAC address
					return GetMacAddr(buf);

				case 1:		// Received packet acquisition (size/buffer)
					if (phase == 0) {
						// Get packet size
						ReceivePacket();
						buf[0] = (BYTE)(packet_len >> 8);
						buf[1] = (BYTE)packet_len;
						return 2;
					} else {
						// Get package data
						GetPacketBuf(buf);
						return packet_len;
					}

				case 2:		// Received packet acquisition (size + buffer simultaneously)
					ReceivePacket();
					buf[0] = (BYTE)(packet_len >> 8);
					buf[1] = (BYTE)packet_len;
					GetPacketBuf(&buf[2]);
					return packet_len + 2;

				case 3:		// Simultaneous acquisition of multiple packets (size + buffer simultaneously)
					// Currently the maximum number of packets is 10
					// Isn't it too fast if I increase more?
					total_len = 0;
					for (i = 0; i < 10; i++) {
						ReceivePacket();
						*buf++ = (BYTE)(packet_len >> 8);
						*buf++ = (BYTE)packet_len;
						total_len += 2;
						if (packet_len == 0)
							break;
						GetPacketBuf(buf);
						buf += packet_len;
						total_len += packet_len;
					}
					return total_len;
			}
			break;
#endif	// RASCSI && !BAREMETAL

		case 2:		// Host Drive
			switch (phase) {
				case 0:		// Get result code
					return ReadFsResult(buf);

				case 1:		// Return data acquisition
					return ReadFsOut(buf);

				case 2:		// Return additional data acquisition
					return ReadFsOpt(buf);
			}
			break;
	}

	// Error
	ASSERT(FALSE);
	return 0;
}

//---------------------------------------------------------------------------
//
//	SEND MESSAGE(10)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIBR::SendMessage10(const DWORD *cdb, BYTE *buf)
{
	int type;
	int func;
	int phase;
	int len;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);

	// Type
	type = cdb[2];

	// Function number
	func = cdb[3];

	// Phase
	phase = cdb[9];

	// Get the number of lights
	len = cdb[6];
	len <<= 8;
	len |= cdb[7];
	len <<= 8;
	len |= cdb[8];

	switch (type) {
#if defined(RASCSI) && !defined(BAREMETAL)
		case 1:		// Ethernet
			// Do not process if TAP is invalid
			if (!m_bTapEnable) {
				return FALSE;
			}

			switch (func) {
				case 0:		// MAC address setting
					SetMacAddr(buf);
					return TRUE;

				case 1:		// Send packet
					SendPacket(buf, len);
					return TRUE;
			}
			break;
#endif	// RASCSI && !BAREMETAL

		case 2:		// Host drive
			switch (phase) {
				case 0:		// issue command
					WriteFs(func, buf);
					return TRUE;

				case 1:		// additional data writing
					WriteFsOpt(buf, len);
					return TRUE;
			}
			break;
	}

	// Error
	ASSERT(FALSE);
	return FALSE;
}

#if defined(RASCSI) && !defined(BAREMETAL)
//---------------------------------------------------------------------------
//
//	Get MAC Address
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::GetMacAddr(BYTE *mac)
{
	ASSERT(this);
	ASSERT(mac);

	memcpy(mac, mac_addr, 6);
	return 6;
}

//---------------------------------------------------------------------------
//
//	Set MAC Address
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::SetMacAddr(BYTE *mac)
{
	ASSERT(this);
	ASSERT(mac);

	memcpy(mac_addr, mac, 6);
}

//---------------------------------------------------------------------------
//
//	Receive Packet
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::ReceivePacket()
{
	static const BYTE bcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	ASSERT(this);
	ASSERT(tap);

	// previous packet has not been received
	if (packet_enable) {
		return;
	}

	// Receive packet
	packet_len = tap->Rx(packet_buf);

	// Check if received packet
	if (memcmp(packet_buf, mac_addr, 6) != 0) {
		if (memcmp(packet_buf, bcast_addr, 6) != 0) {
			packet_len = 0;
			return;
		}
	}

	// Discard if it exceeds the buffer size
	if (packet_len > 2048) {
		packet_len = 0;
		return;
	}

	// Store in receive buffer
	if (packet_len > 0) {
		packet_enable = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Get Packet
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::GetPacketBuf(BYTE *buf)
{
	int len;

	ASSERT(this);
	ASSERT(tap);
	ASSERT(buf);

	// Size limit
	len = packet_len;
	if (len > 2048) {
		len = 2048;
	}

	// Copy
	memcpy(buf, packet_buf, len);

	// Received
	packet_enable = FALSE;
}

//---------------------------------------------------------------------------
//
//	Send Packet
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::SendPacket(BYTE *buf, int len)
{
	ASSERT(this);
	ASSERT(tap);
	ASSERT(buf);

	tap->Tx(buf, len);
}
#endif	// RASCSI && !BAREMETAL

//---------------------------------------------------------------------------
//
//  $40 - Device Boot
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_InitDevice(BYTE *buf)
{
	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	fs->Reset();
	fsresult = fs->InitDevice((Human68k::argument_t*)buf);
}

//---------------------------------------------------------------------------
//
//  $41 - Directory Check
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_CheckDir(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	fsresult = fs->CheckDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $42 - Create Directory
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_MakeDir(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	fsresult = fs->MakeDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $43 - Remove Directory
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_RemoveDir(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	fsresult = fs->RemoveDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $44 - Rename
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Rename(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	Human68k::namests_t* pNamestsNew;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	pNamestsNew = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	fsresult = fs->Rename(nUnit, pNamests, pNamestsNew);
}

//---------------------------------------------------------------------------
//
//  $45 - Delete File
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Delete(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	fsresult = fs->Delete(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $46 - Get / Set file attributes
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Attribute(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	DWORD nHumanAttribute;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	dp = (DWORD*)&buf[i];
	nHumanAttribute = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->Attribute(nUnit, pNamests, nHumanAttribute);
}

//---------------------------------------------------------------------------
//
//  $47 - File Search
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Files(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::namests_t *pNamests;
	Human68k::files_t *files;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	files = (Human68k::files_t*)&buf[i];
	i += sizeof(Human68k::files_t);

	files->sector = ntohl(files->sector);
	files->offset = ntohs(files->offset);
	files->time = ntohs(files->time);
	files->date = ntohs(files->date);
	files->size = ntohl(files->size);

	fsresult = fs->Files(nUnit, nKey, pNamests, files);

	files->sector = htonl(files->sector);
	files->offset = htons(files->offset);
	files->time = htons(files->time);
	files->date = htons(files->date);
	files->size = htonl(files->size);

	i = 0;
	memcpy(&fsout[i], files, sizeof(Human68k::files_t));
	i += sizeof(Human68k::files_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $48 - File next search
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_NFiles(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::files_t *files;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	files = (Human68k::files_t*)&buf[i];
	i += sizeof(Human68k::files_t);

	files->sector = ntohl(files->sector);
	files->offset = ntohs(files->offset);
	files->time = ntohs(files->time);
	files->date = ntohs(files->date);
	files->size = ntohl(files->size);

	fsresult = fs->NFiles(nUnit, nKey, files);

	files->sector = htonl(files->sector);
	files->offset = htons(files->offset);
	files->time = htons(files->time);
	files->date = htons(files->date);
	files->size = htonl(files->size);

	i = 0;
	memcpy(&fsout[i], files, sizeof(Human68k::files_t));
	i += sizeof(Human68k::files_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $49 - File Creation
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Create(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::namests_t *pNamests;
	Human68k::fcb_t *pFcb;
	DWORD nAttribute;
	BOOL bForce;
	DWORD *dp;
	BOOL *bp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	nAttribute = ntohl(*dp);
	i += sizeof(DWORD);

	bp = (BOOL*)&buf[i];
	bForce = ntohl(*bp);
	i += sizeof(BOOL);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Create(nUnit, nKey, pNamests, pFcb, nAttribute, bForce);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $4A - Open File
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Open(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::namests_t *pNamests;
	Human68k::fcb_t *pFcb;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Open(nUnit, nKey, pNamests, pFcb);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $4B - Close File
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Close(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::fcb_t *pFcb;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Close(nUnit, nKey, pFcb);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $4C - Read File
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Read(BYTE *buf)
{
	DWORD nKey;
	Human68k::fcb_t *pFcb;
	DWORD nSize;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	nSize = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Read(nKey, pFcb, fsopt, nSize);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;

	fsoptlen = fsresult;
}

//---------------------------------------------------------------------------
//
//  $4D - Write file
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Write(BYTE *buf)
{
	DWORD nKey;
	Human68k::fcb_t *pFcb;
	DWORD nSize;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	nSize = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Write(nKey, pFcb, fsopt, nSize);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $4E - Seek file
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Seek(BYTE *buf)
{
	DWORD nKey;
	Human68k::fcb_t *pFcb;
	DWORD nMode;
	int nOffset;
	DWORD *dp;
	int *ip;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	nMode = ntohl(*dp);
	i += sizeof(DWORD);

	ip = (int*)&buf[i];
	nOffset = ntohl(*ip);
	i += sizeof(int);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Seek(nKey, pFcb, nMode, nOffset);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $4F - File Timestamp Get / Set
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_TimeStamp(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::fcb_t *pFcb;
	DWORD nHumanTime;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	nHumanTime = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->TimeStamp(nUnit, nKey, pFcb, nHumanTime);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $50 - Get Capacity
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_GetCapacity(BYTE *buf)
{
	DWORD nUnit;
	Human68k::capacity_t cap;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->GetCapacity(nUnit, &cap);

	cap.freearea = htons(cap.freearea);
	cap.clusters = htons(cap.clusters);
	cap.sectors = htons(cap.sectors);
	cap.bytes = htons(cap.bytes);

	memcpy(fsout, &cap, sizeof(Human68k::capacity_t));
	fsoutlen = sizeof(Human68k::capacity_t);
}

//---------------------------------------------------------------------------
//
//  $51 - Drive status inspection/control
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_CtrlDrive(BYTE *buf)
{
	DWORD nUnit;
	Human68k::ctrldrive_t *pCtrlDrive;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pCtrlDrive = (Human68k::ctrldrive_t*)&buf[i];
	i += sizeof(Human68k::ctrldrive_t);

	fsresult = fs->CtrlDrive(nUnit, pCtrlDrive);

	memcpy(fsout, pCtrlDrive, sizeof(Human68k::ctrldrive_t));
	fsoutlen = sizeof(Human68k::ctrldrive_t);
}

//---------------------------------------------------------------------------
//
//  $52 - Get DPB
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_GetDPB(BYTE *buf)
{
	DWORD nUnit;
	Human68k::dpb_t dpb;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->GetDPB(nUnit, &dpb);

	dpb.sector_size = htons(dpb.sector_size);
	dpb.fat_sector = htons(dpb.fat_sector);
	dpb.file_max = htons(dpb.file_max);
	dpb.data_sector = htons(dpb.data_sector);
	dpb.cluster_max = htons(dpb.cluster_max);
	dpb.root_sector = htons(dpb.root_sector);

	memcpy(fsout, &dpb, sizeof(Human68k::dpb_t));
	fsoutlen = sizeof(Human68k::dpb_t);
}

//---------------------------------------------------------------------------
//
//  $53 - Read Sector
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_DiskRead(BYTE *buf)
{
	DWORD nUnit;
	DWORD nSector;
	DWORD nSize;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nSector = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nSize = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->DiskRead(nUnit, fsout, nSector, nSize);
	fsoutlen = 0x200;
}

//---------------------------------------------------------------------------
//
//  $54 - Write Sector
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_DiskWrite(BYTE *buf)
{
	DWORD nUnit;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->DiskWrite(nUnit);
}

//---------------------------------------------------------------------------
//
//  $55 - IOCTRL
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Ioctrl(BYTE *buf)
{
	DWORD nUnit;
	DWORD nFunction;
	Human68k::ioctrl_t *pIoctrl;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nFunction = ntohl(*dp);
	i += sizeof(DWORD);

	pIoctrl = (Human68k::ioctrl_t*)&buf[i];
	i += sizeof(Human68k::ioctrl_t);

	switch (nFunction) {
		case 2:
		case -2:
			pIoctrl->param = htonl(pIoctrl->param);
			break;
	}

	fsresult = fs->Ioctrl(nUnit, nFunction, pIoctrl);

	switch (nFunction) {
		case 0:
			pIoctrl->media = htons(pIoctrl->media);
			break;
		case 1:
		case -3:
			pIoctrl->param = htonl(pIoctrl->param);
			break;
	}

	i = 0;
	memcpy(&fsout[i], pIoctrl, sizeof(Human68k::ioctrl_t));
	i += sizeof(Human68k::ioctrl_t);
	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $56 - Flush
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Flush(BYTE *buf)
{
	DWORD nUnit;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->Flush(nUnit);
}

//---------------------------------------------------------------------------
//
//  $57 - Check Media
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_CheckMedia(BYTE *buf)
{
	DWORD nUnit;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->CheckMedia(nUnit);
}

//---------------------------------------------------------------------------
//
//  $58 - Lock
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Lock(BYTE *buf)
{
	DWORD nUnit;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->Lock(nUnit);
}

//---------------------------------------------------------------------------
//
//	Read Filesystem (result code)
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::ReadFsResult(BYTE *buf)
{
	DWORD *dp;

	ASSERT(this);
	ASSERT(buf);

	dp = (DWORD*)buf;
	*dp = htonl(fsresult);
	return sizeof(DWORD);
}

//---------------------------------------------------------------------------
//
//	Read Filesystem (return data)
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::ReadFsOut(BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	memcpy(buf, fsout, fsoutlen);
	return fsoutlen;
}

//---------------------------------------------------------------------------
//
//	Read file system (return option data)
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::ReadFsOpt(BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	memcpy(buf, fsopt, fsoptlen);
	return fsoptlen;
}

//---------------------------------------------------------------------------
//
//	Write Filesystem
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::WriteFs(int func, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	fsresult = FS_FATAL_INVALIDCOMMAND;
	fsoutlen = 0;
	fsoptlen = 0;

	// コマンド分岐
	func &= 0x1f;
	switch (func) {
		case 0x00: return FS_InitDevice(buf);	// $40 - start device
		case 0x01: return FS_CheckDir(buf);		// $41 - directory check
		case 0x02: return FS_MakeDir(buf);		// $42 - create directory
		case 0x03: return FS_RemoveDir(buf);	// $43 - remove directory
		case 0x04: return FS_Rename(buf);		// $44 - change file name
		case 0x05: return FS_Delete(buf);		// $45 - delete file
		case 0x06: return FS_Attribute(buf);	// $46 - Get/set file attribute
		case 0x07: return FS_Files(buf);		// $47 - file search
		case 0x08: return FS_NFiles(buf);		// $48 - next file search
		case 0x09: return FS_Create(buf);		// $49 - create file
		case 0x0A: return FS_Open(buf);			// $4A - File open
		case 0x0B: return FS_Close(buf);		// $4B - File close
		case 0x0C: return FS_Read(buf);			// $4C - read file
		case 0x0D: return FS_Write(buf);		// $4D - write file
		case 0x0E: return FS_Seek(buf);			// $4E - File seek
		case 0x0F: return FS_TimeStamp(buf);	// $4F - Get/set file modification time
		case 0x10: return FS_GetCapacity(buf);	// $50 - get capacity
		case 0x11: return FS_CtrlDrive(buf);	// $51 - Drive control/state check
		case 0x12: return FS_GetDPB(buf);		// $52 - Get DPB
		case 0x13: return FS_DiskRead(buf);		// $53 - read sector
		case 0x14: return FS_DiskWrite(buf);	// $54 - write sector
		case 0x15: return FS_Ioctrl(buf);		// $55 - IOCTRL
		case 0x16: return FS_Flush(buf);		// $56 - flush
		case 0x17: return FS_CheckMedia(buf);	// $57 - check media exchange
		case 0x18: return FS_Lock(buf);			// $58 - exclusive control
	}
}

//---------------------------------------------------------------------------
//
//	File system write (input option data)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::WriteFsOpt(BYTE *buf, int num)
{
	ASSERT(this);

	memcpy(fsopt, buf, num);
}

//===========================================================================
//
//	SASI Device
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
#ifdef RASCSI
SASIDEV::SASIDEV()
#else
SASIDEV::SASIDEV(Device *dev)
#endif	// RASCSI
{
	int i;

#ifndef RASCSI
	// Remember host device
	host = dev;
#endif	// RASCSI

	// Work initialization
	ctrl.phase = BUS::busfree;
	ctrl.id = -1;
	ctrl.bus = NULL;
	memset(ctrl.cmd, 0x00, sizeof(ctrl.cmd));
	ctrl.status = 0x00;
	ctrl.message = 0x00;
#ifdef RASCSI
	ctrl.execstart = 0;
#endif	// RASCSI
	ctrl.bufsize = 0x800;
	ctrl.buffer = (BYTE *)malloc(ctrl.bufsize);
	memset(ctrl.buffer, 0x00, ctrl.bufsize);
	ctrl.blocks = 0;
	ctrl.next = 0;
	ctrl.offset = 0;
	ctrl.length = 0;

	// Logical unit initialization
	for (i = 0; i < UnitMax; i++) {
		ctrl.unit[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SASIDEV::~SASIDEV()
{
	// Free the buffer
	if (ctrl.buffer) {
		free(ctrl.buffer);
		ctrl.buffer = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Device reset
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Reset()
{
	int i;

	ASSERT(this);

	// Work initialization
	memset(ctrl.cmd, 0x00, sizeof(ctrl.cmd));
	ctrl.phase = BUS::busfree;
	ctrl.status = 0x00;
	ctrl.message = 0x00;
#ifdef RASCSI
	ctrl.execstart = 0;
#endif	// RASCSI
	memset(ctrl.buffer, 0x00, ctrl.bufsize);
	ctrl.blocks = 0;
	ctrl.next = 0;
	ctrl.offset = 0;
	ctrl.length = 0;

	// Unit initialization
	for (i = 0; i < UnitMax; i++) {
		if (ctrl.unit[i]) {
			ctrl.unit[i]->Reset();
		}
	}
}

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIDEV::Save(Fileio *fio, int /*ver*/)
{
	DWORD sz;

	ASSERT(this);
	ASSERT(fio);

	// Save size
	sz = 2120;
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save entity
	PROP_EXPORT(fio, ctrl.phase);
	PROP_EXPORT(fio, ctrl.id);
	PROP_EXPORT(fio, ctrl.cmd);
	PROP_EXPORT(fio, ctrl.status);
	PROP_EXPORT(fio, ctrl.message);
	if (!fio->Write(ctrl.buffer, 0x800)) {
		return FALSE;
	}
	PROP_EXPORT(fio, ctrl.blocks);
	PROP_EXPORT(fio, ctrl.next);
	PROP_EXPORT(fio, ctrl.offset);
	PROP_EXPORT(fio, ctrl.length);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIDEV::Load(Fileio *fio, int ver)
{
	DWORD sz;

	ASSERT(this);
	ASSERT(fio);

	// Not saved before version 3.11
	if (ver <= 0x0311) {
		return TRUE;
	}

	// Load size and check if the size matches
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != 2120) {
		return FALSE;
	}

	// Load the entity
	PROP_IMPORT(fio, ctrl.phase);
	PROP_IMPORT(fio, ctrl.id);
	PROP_IMPORT(fio, ctrl.cmd);
	PROP_IMPORT(fio, ctrl.status);
	PROP_IMPORT(fio, ctrl.message);
	if (!fio->Read(ctrl.buffer, 0x800)) {
		return FALSE;
	}
	PROP_IMPORT(fio, ctrl.blocks);
	PROP_IMPORT(fio, ctrl.next);
	PROP_IMPORT(fio, ctrl.offset);
	PROP_IMPORT(fio, ctrl.length);

	return TRUE;
}
#endif	// RASCSI

//---------------------------------------------------------------------------
//
//	Connect the controller
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Connect(int id, BUS *bus)
{
	ASSERT(this);

	ctrl.id = id;
	ctrl.bus = bus;
}

//---------------------------------------------------------------------------
//
//	Get the logical unit
//
//---------------------------------------------------------------------------
Disk* FASTCALL SASIDEV::GetUnit(int no)
{
	ASSERT(this);
	ASSERT(no < UnitMax);

	return ctrl.unit[no];
}

//---------------------------------------------------------------------------
//
//	Set the logical unit
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::SetUnit(int no, Disk *dev)
{
	ASSERT(this);
	ASSERT(no < UnitMax);

	ctrl.unit[no] = dev;
}

//---------------------------------------------------------------------------
//
//	Check to see if this has a valid logical unit
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIDEV::HasUnit()
{
	int i;

	ASSERT(this);

	for (i = 0; i < UnitMax; i++) {
		if (ctrl.unit[i]) {
			return TRUE;
		}
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Get internal data
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::GetCTRL(ctrl_t *buffer)
{
	ASSERT(this);
	ASSERT(buffer);

	// reference the internal structure
	*buffer = ctrl;
}

//---------------------------------------------------------------------------
//
//	Get a busy unit
//
//---------------------------------------------------------------------------
Disk* FASTCALL SASIDEV::GetBusyUnit()
{
	DWORD lun;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	return ctrl.unit[lun];
}

//---------------------------------------------------------------------------
//
//	Run
//
//---------------------------------------------------------------------------
BUS::phase_t FASTCALL SASIDEV::Process()
{
	ASSERT(this);

	// Do nothing if not connected
	if (ctrl.id < 0 || ctrl.bus == NULL) {
		return ctrl.phase;
	}

	// Get bus information
	ctrl.bus->Aquire();

	// For the monitor tool, we shouldn't need to reset. We're just logging information
	// Reset
	if (ctrl.bus->GetRST()) {
#if defined(DISK_LOG)
		Log(Log::Normal, "RESET signal received");
#endif	// DISK_LOG

		// Reset the controller
		Reset();

		// Reset the bus
		ctrl.bus->Reset();
		return ctrl.phase;
	}

	// Phase processing
	switch (ctrl.phase) {
		// Bus free
		case BUS::busfree:
			BusFree();
			break;

		// Selection
		case BUS::selection:
			Selection();
			break;

		// Data out (MCI=000)
		case BUS::dataout:
			DataOut();
			break;

		// Data in (MCI=001)
		case BUS::datain:
			DataIn();
			break;

		// Command (MCI=010)
		case BUS::command:
			Command();
			break;

		// Status (MCI=011)
		case BUS::status:
			Status();
			break;

		// Msg in (MCI=111)
		case BUS::msgin:
			MsgIn();
			break;

		// Other
		default:
			ASSERT(FALSE);
			break;
	}

	return ctrl.phase;
}

//---------------------------------------------------------------------------
//
//	Bus free phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::BusFree()
{
	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::busfree) {

#if defined(DISK_LOG)
		Log(Log::Normal, "Bus free phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::busfree;

		// Set Signal lines
		ctrl.bus->SetREQ(FALSE);
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(FALSE);
		ctrl.bus->SetIO(FALSE);
		ctrl.bus->SetBSY(FALSE);

		// Initialize status and message
		ctrl.status = 0x00;
		ctrl.message = 0x00;
		return;
	}

	// Move to selection phase
	if (ctrl.bus->GetSEL() && !ctrl.bus->GetBSY()) {
		Selection();
	}
}

//---------------------------------------------------------------------------
//
//	Selection phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Selection()
{
	DWORD id;

	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::selection) {
		// Invalid if IDs do not match
		id = 1 << ctrl.id;
		if ((ctrl.bus->GetDAT() & id) == 0) {
			return;
		}

		// Return if there is no unit
		if (!HasUnit()) {
			return;
		}

#if defined(DISK_LOG)
		Log(Log::Normal,
			"Selection Phase ID=%d (with device)", ctrl.id);
#endif	// DISK_LOG

		// Phase change
		ctrl.phase = BUS::selection;

		// Raiase BSY and respond
		ctrl.bus->SetBSY(TRUE);
		return;
	}

	// Command phase shifts when selection is completed
	if (!ctrl.bus->GetSEL() && ctrl.bus->GetBSY()) {
		Command();
	}
}

//---------------------------------------------------------------------------
//
//	Command phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Command()
{
#ifdef RASCSI
	int count;
	int i;
#endif	// RASCSI

	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::command) {

#if defined(DISK_LOG)
		Log(Log::Normal, "Command Phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::command;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(FALSE);

		// Data transfer is 6 bytes x 1 block
		ctrl.offset = 0;
		ctrl.length = 6;
		ctrl.blocks = 1;

#ifdef RASCSI
		// Command reception handshake (10 bytes are automatically received at the first command)
		count = ctrl.bus->CommandHandShake(ctrl.buffer);
		//** printf("Command received: " );
		//** for(int i=0; i< count; i++)
		//** {
		//**    printf("%02X ", ctrl.buffer[i]);
		//** }
		//** printf("\n");

		// If no byte can be received move to the status phase
		if (count == 0) {
			Error();
			return;
		}

		// Check 10-byte CDB
		if (ctrl.buffer[0] >= 0x20 && ctrl.buffer[0] <= 0x7D) {
			ctrl.length = 10;
		}

		// If not able to receive all, move to the status phase
		if (count != (int)ctrl.length) {
			Error();
			return;
		}

		// Command data transfer
		for (i = 0; i < (int)ctrl.length; i++) {
			ctrl.cmd[i] = (DWORD)ctrl.buffer[i];
		}

		// Clear length and block
		ctrl.length = 0;
		ctrl.blocks = 0;

		// Execution Phase
		Execute();
#else
		// Request the command
		ctrl.bus->SetREQ(TRUE);
		return;
#endif	// RASCSI
	}
#ifndef RASCSI
	// Requesting
	if (ctrl.bus->GetREQ()) {
		// Sent by the initiator
		if (ctrl.bus->GetACK()) {
			Receive();
		}
	} else {
		// Request the initator to
		if (!ctrl.bus->GetACK()) {
			ReceiveNext();
		}
	}
#endif	// RASCSI
}

//---------------------------------------------------------------------------
//
//	Execution Phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Execute()
{
	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "Execution Phase Command %02X", ctrl.cmd[0]);
#endif	// DISK_LOG

	// Phase Setting
	ctrl.phase = BUS::execute;

	// Initialization for data transfer
	ctrl.offset = 0;
	ctrl.blocks = 1;
#ifdef RASCSI
	ctrl.execstart = SysTimer::GetTimerLow();
#endif	// RASCSI

	// Process by command
	switch (ctrl.cmd[0]) {
		// TEST UNIT READY
		case 0x00:
			CmdTestUnitReady();
			return;

		// REZERO UNIT
		case 0x01:
			CmdRezero();
			return;

		// REQUEST SENSE
		case 0x03:
			CmdRequestSense();
			return;

		// FORMAT UNIT
		case 0x04:
			CmdFormat();
			return;

		// FORMAT UNIT
		case 0x06:
			CmdFormat();
			return;

		// REASSIGN BLOCKS
		case 0x07:
			CmdReassign();
			return;

		// READ(6)
		case 0x08:
			CmdRead6();
			return;

		// WRITE(6)
		case 0x0a:
			CmdWrite6();
			return;

		// SEEK(6)
		case 0x0b:
			CmdSeek6();
			return;

		// ASSIGN(SASIのみ)
		case 0x0e:
			CmdAssign();
			return;

		// SPECIFY(SASIのみ)
		case 0xc2:
			CmdSpecify();
			return;
	}

	// Unsupported command
	Log(Log::Warning, "Unsupported command $%02X", ctrl.cmd[0]);
	CmdInvalid();
}

//---------------------------------------------------------------------------
//
//	Status phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Status()
{
#ifdef RASCSI
	DWORD min_exec_time;
	DWORD time;
#endif	// RASCSI

	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::status) {

#ifdef RASCSI
		// Minimum execution time
		if (ctrl.execstart > 0) {
			min_exec_time = IsSASI() ? min_exec_time_sasi : min_exec_time_scsi;
			time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < min_exec_time) {
				SysTimer::SleepUsec(min_exec_time - time);
			}
			ctrl.execstart = 0;
		} else {
			SysTimer::SleepUsec(5);
		}
#endif	// RASCSI

#if defined(DISK_LOG)
		Log(Log::Normal, "Status phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::status;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(TRUE);

		// Data transfer is 1 byte x 1 block
		ctrl.offset = 0;
		ctrl.length = 1;
		ctrl.blocks = 1;
		ctrl.buffer[0] = (BYTE)ctrl.status;

#ifndef RASCSI
		// Request status
//		ctrl.bus->SetDAT(ctrl.buffer[0]);
//		ctrl.bus->SetREQ(TRUE);

#if defined(DISK_LOG)
		Log(Log::Normal, "Status Phase $%02X", ctrl.status);
#endif	// DISK_LOG
#endif	// RASCSI
		return;
	}

#ifdef RASCSI
	// Send
	Send();
#else
	// Requesting
	if (ctrl.bus->GetREQ()) {
		// Initiator received
		if (ctrl.bus->GetACK()) {
			SendNext();
		}
	} else {
		// Initiator requests next
		if (!ctrl.bus->GetACK()) {
			Send();
		}
	}
#endif	// RASCSI
}

//---------------------------------------------------------------------------
//
//	Message in phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::MsgIn()
{
	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::msgin) {

#if defined(DISK_LOG)
		Log(Log::Normal, "Message in phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::msgin;

		// Signal line operated by the target
		ctrl.bus->SetMSG(TRUE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(TRUE);

		// length, blocks are already set
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.blocks > 0);
		ctrl.offset = 0;

#ifndef RASCSI
		// Request message
		ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset]);
		ctrl.bus->SetREQ(TRUE);

#if defined(DISK_LOG)
		Log(Log::Normal, "Message in phase $%02X", ctrl.buffer[ctrl.offset]);
#endif	// DISK_LOG
#endif	// RASCSI
		return;
	}

#ifdef RASCSI
	//Send
	Send();
#else
	// Requesting
	if (ctrl.bus->GetREQ()) {
		// Initator received
		if (ctrl.bus->GetACK()) {
			SendNext();
		}
	} else {
		// Initiator requests next
		if (!ctrl.bus->GetACK()) {
			Send();
		}
	}
#endif	// RASCSI
}

//---------------------------------------------------------------------------
//
//	Data-in Phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::DataIn()
{
#ifdef RASCSI
	DWORD min_exec_time;
	DWORD time;
#endif	// RASCSI

	ASSERT(this);
	ASSERT(ctrl.length >= 0);

	// Phase change
	if (ctrl.phase != BUS::datain) {

#ifdef RASCSI
		// Minimum execution time
		if (ctrl.execstart > 0) {
			min_exec_time = IsSASI() ? min_exec_time_sasi : min_exec_time_scsi;
			time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < min_exec_time) {
				SysTimer::SleepUsec(min_exec_time - time);
			}
			ctrl.execstart = 0;
		}
#endif	// RASCSI

		// If the length is 0, go to the status phase
		if (ctrl.length == 0) {
			Status();
			return;
		}

#if defined(DISK_LOG)
		Log(Log::Normal, "Data-in Phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::datain;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(FALSE);
		ctrl.bus->SetIO(TRUE);

		// length, blocks are already set
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.blocks > 0);
		ctrl.offset = 0;

#ifndef RASCSI
		// Assert the DAT signal
		ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset]);

		// Request data
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

#ifdef RASCSI
	// Send
	Send();
#else
	// Requesting
	if (ctrl.bus->GetREQ()) {
		// Initator received
		if (ctrl.bus->GetACK()) {
			SendNext();
		}
	} else {
		// Initiator requests next
		if (!ctrl.bus->GetACK()) {
			Send();
		}
	}
#endif	// RASCSI
}

//---------------------------------------------------------------------------
//
//	Data out phase
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::DataOut()
{
#ifdef RASCSI
	DWORD min_exec_time;
	DWORD time;
#endif	// RASCSI

	ASSERT(this);
	ASSERT(ctrl.length >= 0);

	// Phase change
	if (ctrl.phase != BUS::dataout) {

#ifdef RASCSI
		// Minimum execution time
		if (ctrl.execstart > 0) {
			min_exec_time = IsSASI() ? min_exec_time_sasi : min_exec_time_scsi;
			time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < min_exec_time) {
				SysTimer::SleepUsec(min_exec_time - time);
			}
			ctrl.execstart = 0;
		}
#endif	// RASCSI

		// If the length is 0, go to the status phase
		if (ctrl.length == 0) {
			Status();
			return;
		}

#if defined(DISK_LOG)
		Log(Log::Normal, "Data out phase");
#endif	// DISK_LOG

		// Phase Setting
		ctrl.phase = BUS::dataout;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(FALSE);
		ctrl.bus->SetIO(FALSE);

		// length, blocks are already calculated
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.blocks > 0);
		ctrl.offset = 0;

#ifndef	RASCSI
		// Request data
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

#ifdef	RASCSI
	// Receive
	Receive();
#else
	// Requesting
	if (ctrl.bus->GetREQ()) {
		// Sent by the initiator
		if (ctrl.bus->GetACK()) {
			Receive();
		}
	} else {
		// Request the initator to
		if (!ctrl.bus->GetACK()) {
			ReceiveNext();
		}
	}
#endif	// RASCSI
}

//---------------------------------------------------------------------------
//
//	Error
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Error()
{
	DWORD lun;

	ASSERT(this);

	// Get bus information
	ctrl.bus->Aquire();

	// Reset check
	if (ctrl.bus->GetRST()) {
		// Reset the controller
		Reset();

		// Reset the bus
		ctrl.bus->Reset();
		return;
	}

	// Bus free for status phase and message in phase
	if (ctrl.phase == BUS::status || ctrl.phase == BUS::msgin) {
		BusFree();
		return;
	}

#if defined(DISK_LOG)
	Log(Log::Warning, "Error occured (going to status phase)");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;

	// Set status and message(CHECK CONDITION)
	ctrl.status = (lun << 5) | 0x02;

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdTestUnitReady()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "TEST UNIT READY Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->TestUnitReady(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	REZERO UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdRezero()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "REZERO UNIT Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Rezero(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdRequestSense()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "REQUEST SENSE Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->RequestSense(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length > 0);

#if defined(DISK_LOG)
	Log(Log::Normal, "Sense key $%02X", ctrl.buffer[2]);
#endif	// DISK_LOG

	// Read phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	FORMAT UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdFormat()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "FORMAT UNIT Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Format(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	REASSIGN BLOCKS
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdReassign()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "REASSIGN BLOCKS Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Reassign(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	READ(6)
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdRead6()
{
	DWORD lun;
	DWORD record;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Get record number and block number
	record = ctrl.cmd[1] & 0x1f;
	record <<= 8;
	record |= ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	ctrl.blocks = ctrl.cmd[4];
	if (ctrl.blocks == 0) {
		ctrl.blocks = 0x100;
	}

#if defined(DISK_LOG)
	Log(Log::Normal,
		"READ(6) command record=%06X blocks=%d", record, ctrl.blocks);
#endif	// DISK_LOG

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->Read(ctrl.buffer, record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Read phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	WRITE(6)
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdWrite6()
{
	DWORD lun;
	DWORD record;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Get record number and block number
	record = ctrl.cmd[1] & 0x1f;
	record <<= 8;
	record |= ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	ctrl.blocks = ctrl.cmd[4];
	if (ctrl.blocks == 0) {
		ctrl.blocks = 0x100;
	}

#if defined(DISK_LOG)
	Log(Log::Normal,
		"WRITE(6) command record=%06X blocks=%d", record, ctrl.blocks);
#endif	// DISK_LOG

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->WriteCheck(record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Write phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	SEEK(6)
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdSeek6()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "SEEK(6) Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Seek(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	ASSIGN
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdAssign()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "ASSIGN Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Assign(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// 4Request 4 bytes of data
	ctrl.length = 4;

	// Write phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	SPECIFY
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdSpecify()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "SPECIFY Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Assign(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// Request 10 bytes of data
	ctrl.length = 10;

	// Write phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	Unsupported command
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::CmdInvalid()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "Command not supported");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (ctrl.unit[lun]) {
		// Command processing on drive
		ctrl.unit[lun]->InvalidCmd();
	}

	// Failure (Error)
	Error();
}

//===========================================================================
//
//	Data transfer
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Data transmission
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Send()
{
#ifdef RASCSI
	int len;
#endif	// RASCSI
	BOOL result;

	ASSERT(this);
	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(ctrl.bus->GetIO());

#ifdef RASCSI
	// Check that the length isn't 0
	if (ctrl.length != 0) {
		len = ctrl.bus->SendHandShake(
			&ctrl.buffer[ctrl.offset], ctrl.length);

		// If you can not send it all, move on to the status phase
		if (len != (int)ctrl.length) {
			Error();
			return;
		}

		// Offset and Length
		ctrl.offset += ctrl.length;
		ctrl.length = 0;
		return;
	}
#else
	// Offset and Length
	ASSERT(ctrl.length >= 1);
	ctrl.offset++;
	ctrl.length--;

	// Immediately after ACK is asserted, if the data
	// has been set by SendNext, raise the request
	if (ctrl.length != 0) {
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
		return;
	}
#endif	// RASCSI

	// Remove block and initialize the result
	ctrl.blocks--;
	result = TRUE;

	// Process after data collection (read/data-in only)
	if (ctrl.phase == BUS::datain) {
		if (ctrl.blocks != 0) {
			// Set next buffer (set offset, length)
			result = XferIn(ctrl.buffer);
			//** printf("xfer in: %d \n",result);

#ifndef RASCSI
			ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset]);
#endif	// RASCSI
		}
	}

	// If result FALSE, move to the status phase
	if (!result) {
		Error();
		return;
	}

	// Continue sending if block != 0
	if (ctrl.blocks != 0){
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.offset == 0);
#ifndef RASCSI
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

	// Move to the next phase
	switch (ctrl.phase) {
		// Message in phase
		case BUS::msgin:
			// Bus free phase
			BusFree();
			break;

		// Data-in Phase
		case BUS::datain:
			// status phase
			Status();
			break;

		// Status phase
		case BUS::status:
			// Message in phase
			ctrl.length = 1;
			ctrl.blocks = 1;
			ctrl.buffer[0] = (BYTE)ctrl.message;
			MsgIn();
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}

#ifndef	RASCSI
//---------------------------------------------------------------------------
//
//	Continue sending data
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::SendNext()
{
	ASSERT(this);

	// Req is up
	ASSERT(ctrl.bus->GetREQ());
	ASSERT(ctrl.bus->GetIO());

	// Signal line operated by the target
	ctrl.bus->SetREQ(FALSE);

	// If there is data in the buffer, set it first.
	if (ctrl.length > 1) {
		ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset + 1]);
	}
}
#endif	// RASCSI

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Receive data
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Receive()
{
	DWORD data;

	ASSERT(this);

	// Req is up
	ASSERT(ctrl.bus->GetREQ());
	ASSERT(!ctrl.bus->GetIO());

	// Get data
	data = (DWORD)ctrl.bus->GetDAT();

	// Signal line operated by the target
	ctrl.bus->SetREQ(FALSE);

	switch (ctrl.phase) {
		// Command phase
		case BUS::command:
			ctrl.cmd[ctrl.offset] = data;
#if defined(DISK_LOG)
			Log(Log::Normal, "Command phase $%02X", data);
#endif	// DISK_LOG

			// Set the length again with the first data (offset 0)
			if (ctrl.offset == 0) {
				if (ctrl.cmd[0] >= 0x20 && ctrl.cmd[0] <= 0x7D) {
					// 10 byte CDB
					ctrl.length = 10;
				}
			}
			break;

		// Data out phase
		case BUS::dataout:
			ctrl.buffer[ctrl.offset] = (BYTE)data;
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}
#endif	// RASCSI

#ifdef RASCSI
//---------------------------------------------------------------------------
//
//	Receive data
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Receive()
#else
//---------------------------------------------------------------------------
//
//	Continue receiving data
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::ReceiveNext()
#endif	// RASCSI
{
#ifdef RASCSI
	int len;
#endif	// RASCSI
	BOOL result;

	ASSERT(this);

	// REQ is low
	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(!ctrl.bus->GetIO());

#ifdef RASCSI
	// Length != 0 if received
	if (ctrl.length != 0) {
		// Receive
		len = ctrl.bus->ReceiveHandShake(
			&ctrl.buffer[ctrl.offset], ctrl.length);

		// If not able to receive all, move to status phase
		if (len != (int)ctrl.length) {
			Error();
			return;
		}

		// Offset and Length
		ctrl.offset += ctrl.length;
		ctrl.length = 0;
		return;
	}
#else
	// Offset and Length
	ASSERT(ctrl.length >= 1);
	ctrl.offset++;
	ctrl.length--;

	// If length != 0, set req again
	if (ctrl.length != 0) {
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
		return;
	}
#endif	// RASCSI

	// Remove the control block and initialize the result
	ctrl.blocks--;
	result = TRUE;

	// Process the data out phase
	if (ctrl.phase == BUS::dataout) {
		if (ctrl.blocks == 0) {
			// End with this buffer
			result = XferOut(FALSE);
		} else {
			// Continue to next buffer (set offset, length)
			result = XferOut(TRUE);
		}
	}

	// If result is false, move to the status phase
	if (!result) {
		Error();
		return;
	}

	// Continue to receive is block != 0
	if (ctrl.blocks != 0){
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.offset == 0);
#ifndef RASCSI
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

	// Move to the next phase
	switch (ctrl.phase) {
#ifndef RASCSI
		// Command phase
		case BUS::command:
			// Execution Phase
			Execute();
			break;
#endif	// RASCSI

		// Data out phase
		case BUS::dataout:
			// Flush
			FlushUnit();

			// status phase
			Status();
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Data transfer IN
//	*Reset offset and length
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIDEV::XferIn(BYTE *buf)
{
	DWORD lun;

	ASSERT(this);
	ASSERT(ctrl.phase == BUS::datain);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		return FALSE;
	}

	// Limited to read commands
	switch (ctrl.cmd[0]) {
		// READ(6)
		case 0x08:
		// READ(10)
		case 0x28:
			// Read from disk
			ctrl.length = ctrl.unit[lun]->Read(buf, ctrl.next);
			ctrl.next++;

			//** printf("XferIn read data from disk: ");
			//** for (int i=0; i<ctrl.length; i++)
			//** {
                //** printf("%02X ", ctrl.buffer[i]);
			//** }
			//** printf("\n");

			// If there is an error, go to the status phase
			if (ctrl.length <= 0) {
				// Cancel data-in
				return FALSE;
			}

			// If things are normal, work setting
			ctrl.offset = 0;
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			return FALSE;
	}

	// Succeeded in setting the buffer
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Data transfer OUT
//	*If cont=true, reset the offset and length
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIDEV::XferOut(BOOL cont)
{
	DWORD lun;
	SCSIBR *bridge;

	ASSERT(this);
	ASSERT(ctrl.phase == BUS::dataout);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		return FALSE;
	}

	// MODE SELECT or WRITE system
	switch (ctrl.cmd[0]) {
		// MODE SELECT
		case 0x15:
		// MODE SELECT(10)
		case 0x55:
			if (!ctrl.unit[lun]->ModeSelect(
				ctrl.cmd, ctrl.buffer, ctrl.offset)) {
				// MODE SELECT failed
				return FALSE;
			}
			break;

		// WRITE(6)
		case 0x0a:
		// WRITE(10)
		case 0x2a:
			// Replace the host bridge with SEND MESSAGE 10
			if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'B', 'R')) {
				bridge = (SCSIBR*)ctrl.unit[lun];
				if (!bridge->SendMessage10(ctrl.cmd, ctrl.buffer)) {
					// write failed
					return FALSE;
				}

				// If normal, work setting
				ctrl.offset = 0;
				break;
			}

		// WRITE AND VERIFY
		case 0x2e:
			// Write
			if (!ctrl.unit[lun]->Write(ctrl.buffer, ctrl.next - 1)) {
				// Write failed
				return FALSE;
			}

			// If you do not need the next block, end here
			ctrl.next++;
			if (!cont) {
				break;
			}

			// Check the next block
			ctrl.length = ctrl.unit[lun]->WriteCheck(ctrl.next - 1);
			if (ctrl.length <= 0) {
				// Cannot write
				return FALSE;
			}

			// If normal, work setting
			ctrl.offset = 0;
			break;

		// SPECIFY(SASI only)
		case 0xc2:
			break;

		default:
			ASSERT(FALSE);
			break;
	}

	// Buffer saved successfully
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Logical unit flush
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::FlushUnit()
{
	DWORD lun;

	ASSERT(this);
	ASSERT(ctrl.phase == BUS::dataout);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		return;
	}

	// WRITE system only
	switch (ctrl.cmd[0]) {
		// WRITE(6)
		case 0x0a:
		// WRITE(10)
		case 0x2a:
		// WRITE AND VERIFY
		case 0x2e:
			// Flush
			if (!ctrl.unit[lun]->IsCacheWB()) {
				ctrl.unit[lun]->Flush();
			}
			break;
		default:
            printf("Received an invalid flush command %02X!!!!!\n",ctrl.cmd[0]);
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Get the current phase as a string
//
//---------------------------------------------------------------------------
void SASIDEV::GetPhaseStr(char *str)
{
    switch(this->GetPhase())
    {
        case BUS::busfree:
        strcpy(str,"busfree    ");
        break;
        case BUS::arbitration:
        strcpy(str,"arbitration");
        break;
        case BUS::selection:
        strcpy(str,"selection  ");
        break;
        case BUS::reselection:
        strcpy(str,"reselection");
        break;
        case BUS::command:
        strcpy(str,"command    ");
        break;
        case BUS::execute:
        strcpy(str,"execute    ");
        break;
        case BUS::datain:
        strcpy(str,"datain     ");
        break;
        case BUS::dataout:
        strcpy(str,"dataout    ");
        break;
        case BUS::status:
        strcpy(str,"status     ");
        break;
        case BUS::msgin:
        strcpy(str,"msgin      ");
        break;
        case BUS::msgout:
        strcpy(str,"msgout     ");
        break;
        case BUS::reserved:
        strcpy(str,"reserved   ");
        break;
    }
}

//---------------------------------------------------------------------------
//
//	Log output
//
//---------------------------------------------------------------------------
void FASTCALL SASIDEV::Log(Log::loglevel level, const char *format, ...)
{
#if !defined(BAREMETAL)
	char buffer[0x200];
	char buffer2[0x250];
	char buffer3[0x250];
	char phase_str[20];
	va_list args;
	va_start(args, format);

	if(this->GetID() != 6)
	{
        return;
	}

#ifdef RASCSI
#ifndef DISK_LOG
	if (level == Log::Warning) {
		return;
	}
#endif	// DISK_LOG
#endif	// RASCSI

	// format
	vsprintf(buffer, format, args);

	// end variable length argument
	va_end(args);

	// Add the date/timestamp
	// current date/time based on current system
   time_t now = time(0);
   // convert now to string form
   char* dt = ctime(&now);


   strcpy(buffer2, "[");
   strcat(buffer2, dt);
   // Get rid of the carriage return
   buffer2[strlen(buffer2)-1] = '\0';
   strcat(buffer2, "] ");

   // Get the phase
   this->GetPhaseStr(phase_str);
   sprintf(buffer3, "[%d][%s] ", this->GetID(), phase_str);
   strcat(buffer2,buffer3);
   strcat(buffer2, buffer);


	// Log output
#ifdef RASCSI
	printf("%s\n", buffer2);
#else
	host->GetVM()->GetLog()->Format(level, host, buffer);
#endif	// RASCSI
#endif	// BAREMETAL
}

//===========================================================================
//
//	SCSI Device
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
#ifdef RASCSI
SCSIDEV::SCSIDEV() : SASIDEV()
#else
SCSIDEV::SCSIDEV(Device *dev) : SASIDEV(dev)
#endif
{
	// Synchronous transfer work initialization
	scsi.syncenable = FALSE;
	scsi.syncperiod = 50;
	scsi.syncoffset = 0;
	scsi.atnmsg = FALSE;
	scsi.msc = 0;
	memset(scsi.msb, 0x00, sizeof(scsi.msb));
}

//---------------------------------------------------------------------------
//
//	Device reset
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Reset()
{
	ASSERT(this);

	// Work initialization
	scsi.atnmsg = FALSE;
	scsi.msc = 0;
	memset(scsi.msb, 0x00, sizeof(scsi.msb));

	// Base class
	SASIDEV::Reset();
}

//---------------------------------------------------------------------------
//
//	Process
//
//---------------------------------------------------------------------------
BUS::phase_t FASTCALL SCSIDEV::Process()
{
	ASSERT(this);
	//** printf("SCSIDEV::Process() %d\n", ctrl.id);

	// Do nothing if not connected
	if (ctrl.id < 0 || ctrl.bus == NULL) {
		return ctrl.phase;
	}

	// Get bus information
	ctrl.bus->Aquire();

	// Reset
	if (ctrl.bus->GetRST()) {
#if defined(DISK_LOG)
		Log(Log::Normal, "RESET信号受信");
#endif	// DISK_LOG

		// Reset the controller
		Reset();

		// Reset the bus
		ctrl.bus->Reset();
		return ctrl.phase;
	}

	// Phase processing
	switch (ctrl.phase) {
		// Bus free phase
		case BUS::busfree:
			BusFree();
			break;

		// Selection phase
		case BUS::selection:
			Selection();
			break;

		// Data out (MCI=000)
		case BUS::dataout:
			DataOut();
			break;

		// Data in (MCI=001)
		case BUS::datain:
			DataIn();
			break;

		// Command (MCI=010)
		case BUS::command:
			Command();
			break;

		// Status (MCI=011)
		case BUS::status:
			Status();
			break;

		// Message out (MCI=110)
		case BUS::msgout:
			MsgOut();
			break;

		// Message in (MCI=111)
		case BUS::msgin:
			MsgIn();
			break;

		// Other
		default:
			ASSERT(FALSE);
			break;
	}

	return ctrl.phase;
}

//---------------------------------------------------------------------------
//
//	Phaes
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	Bus free phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::BusFree()
{
	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::busfree) {

#if defined(DISK_LOG)
		Log(Log::Normal, "Bus free phase");
#endif	// DISK_LOG

		// Phase setting
		ctrl.phase = BUS::busfree;

		// Signal line
		ctrl.bus->SetREQ(FALSE);
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(FALSE);
		ctrl.bus->SetIO(FALSE);
		ctrl.bus->SetBSY(FALSE);

		// Initialize status and message
		ctrl.status = 0x00;
		ctrl.message = 0x00;

		// Initialize ATN message reception status
		scsi.atnmsg = FALSE;
		return;
	}

	// Move to selection phase
	if (ctrl.bus->GetSEL() && !ctrl.bus->GetBSY()) {
		Selection();
	}
}

//---------------------------------------------------------------------------
//
//	Selection Phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Selection()
{
	DWORD id;

	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::selection) {
		// invalid if IDs do not match
		id = 1 << ctrl.id;
		if ((ctrl.bus->GetDAT() & id) == 0) {
			return;
		}

		// End if there is no valid unit
		if (!HasUnit()) {
			return;
		}

#if defined(DISK_LOG)
		Log(Log::Normal,
			"Selection Phase ID=%d (with device)", ctrl.id);
#endif	// DISK_LOG

		// Phase setting
		ctrl.phase = BUS::selection;

		// Raise BSY and respond
		ctrl.bus->SetBSY(TRUE);
		return;
	}

	// Selection completed
	if (!ctrl.bus->GetSEL() && ctrl.bus->GetBSY()) {
		// Message out phase if ATN=1, otherwise command phase
		if (ctrl.bus->GetATN()) {
			MsgOut();
		} else {
			Command();
		}
	}
}

//---------------------------------------------------------------------------
//
//	Execution Phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Execute()
{
	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "Execution phase command $%02X", ctrl.cmd[0]);
#endif	// DISK_LOG

	// Phase Setting
	ctrl.phase = BUS::execute;

	// Initialization for data transfer
	ctrl.offset = 0;
	ctrl.blocks = 1;
#ifdef RASCSI
	ctrl.execstart = SysTimer::GetTimerLow();
#endif	// RASCSI

	// Process by command
	switch (ctrl.cmd[0]) {
		// TEST UNIT READY
		case 0x00:
			CmdTestUnitReady();
			return;

		// REZERO
		case 0x01:
			CmdRezero();
			return;

		// REQUEST SENSE
		case 0x03:
			CmdRequestSense();
			return;

		// FORMAT UNIT
		case 0x04:
			CmdFormat();
			return;

		// REASSIGN BLOCKS
		case 0x07:
			CmdReassign();
			return;

		// READ(6)
		case 0x08:
			CmdRead6();
			return;

		// WRITE(6)
		case 0x0a:
			CmdWrite6();
			return;

		// SEEK(6)
		case 0x0b:
			CmdSeek6();
			return;

		// INQUIRY
		case 0x12:
			CmdInquiry();
			return;

		// MODE SELECT
		case 0x15:
			CmdModeSelect();
			return;

		// MDOE SENSE
		case 0x1a:
			CmdModeSense();
			return;

		// START STOP UNIT
		case 0x1b:
			CmdStartStop();
			return;

		// SEND DIAGNOSTIC
		case 0x1d:
			CmdSendDiag();
			return;

		// PREVENT/ALLOW MEDIUM REMOVAL
		case 0x1e:
			CmdRemoval();
			return;

		// READ CAPACITY
		case 0x25:
			CmdReadCapacity();
			return;

		// READ(10)
		case 0x28:
			CmdRead10();
			return;

		// WRITE(10)
		case 0x2a:
			CmdWrite10();
			return;

		// SEEK(10)
		case 0x2b:
			CmdSeek10();
			return;

		// WRITE and VERIFY
		case 0x2e:
			CmdWrite10();
			return;

		// VERIFY
		case 0x2f:
			CmdVerify();
			return;

		// SYNCHRONIZE CACHE
		case 0x35:
			CmdSynchronizeCache();
			return;

		// READ DEFECT DATA(10)
		case 0x37:
			CmdReadDefectData10();
			return;

		// READ TOC
		case 0x43:
			CmdReadToc();
			return;

		// PLAY AUDIO(10)
		case 0x45:
			CmdPlayAudio10();
			return;

		// PLAY AUDIO MSF
		case 0x47:
			CmdPlayAudioMSF();
			return;

		// PLAY AUDIO TRACK
		case 0x48:
			CmdPlayAudioTrack();
			return;

		// MODE SELECT(10)
		case 0x55:
			CmdModeSelect10();
			return;

		// MDOE SENSE(10)
		case 0x5a:
			CmdModeSense10();
			return;

		// SPECIFY (SASI only/Suppress warning when using SxSI)
		case 0xc2:
			CmdInvalid();
			return;

	}

	// No other support
	Log(Log::Normal, "Unsupported command received: $%02X", ctrl.cmd[0]);
	CmdInvalid();
}

//---------------------------------------------------------------------------
//
//	Message out phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::MsgOut()
{
	ASSERT(this);

	// Phase change
	if (ctrl.phase != BUS::msgout) {

#if defined(DISK_LOG)
		Log(Log::Normal, "Message Out Phase");
#endif	// DISK_LOG

		// Message out phase after selection
        // process the IDENTIFY message
		if (ctrl.phase == BUS::selection) {
			scsi.atnmsg = TRUE;
			scsi.msc = 0;
			memset(scsi.msb, 0x00, sizeof(scsi.msb));
		}

		// Phase Setting
		ctrl.phase = BUS::msgout;

		// Signal line operated by the target
		ctrl.bus->SetMSG(TRUE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(FALSE);

		// Data transfer is 1 byte x 1 block
		ctrl.offset = 0;
		ctrl.length = 1;
		ctrl.blocks = 1;

#ifndef RASCSI
		// Request message
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

#ifdef RASCSI
	// Receive
	Receive();
#else
	// Requesting
	if (ctrl.bus->GetREQ()) {
		// Sent by the initiator
		if (ctrl.bus->GetACK()) {
			Receive();
		}
	} else {
		// Request the initator to
		if (!ctrl.bus->GetACK()) {
			ReceiveNext();
		}
	}
#endif	// RASCSI
}

//---------------------------------------------------------------------------
//
//	Common Error Handling
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Error()
{
	ASSERT(this);

	// Get bus information
	ctrl.bus->Aquire();

	// Reset check
	if (ctrl.bus->GetRST()) {
		// Reset the controller
		Reset();

		// Reset the bus
		ctrl.bus->Reset();
		return;
	}

	// Bus free for status phase and message in phase
	if (ctrl.phase == BUS::status || ctrl.phase == BUS::msgin) {
		BusFree();
		return;
	}

#if defined(DISK_LOG)
	Log(Log::Normal, "Error (to status phase)");
#endif	// DISK_LOG

	// Set status and message(CHECK CONDITION)
	ctrl.status = 0x02;
	ctrl.message = 0x00;

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	Command
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdInquiry()
{
	Disk *disk;
	int lun;
	DWORD major;
	DWORD minor;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "INQUIRY Command");
#endif	// DISK_LOG

	// Find a valid unit
	disk = NULL;
	for (lun = 0; lun < UnitMax; lun++) {
		if (ctrl.unit[lun]) {
			disk = ctrl.unit[lun];
			break;
		}
	}

	// Processed on the disk side (it is originally processed by the controller)
	if (disk) {
#ifdef RASCSI
		major = (DWORD)(RASCSI >> 8);
		minor = (DWORD)(RASCSI & 0xff);
#else
		host->GetVM()->GetVersion(major, minor);
#endif	// RASCSI
		ctrl.length =
			ctrl.unit[lun]->Inquiry(ctrl.cmd, ctrl.buffer, major, minor);
	} else {
		ctrl.length = 0;
	}

	if (ctrl.length <= 0) {
		// failure (error)
		Error();
		return;
	}

	// Add synchronous transfer support information
	if (scsi.syncenable) {
		ctrl.buffer[7] |= (1 << 4);
	}

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdModeSelect()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "MODE SELECT Command");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->SelectCheck(ctrl.cmd);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	MODE SENSE
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdModeSense()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "MODE SENSE Command ");
#endif	// DISK_LOG

    //** printf("Received a Mode Sense command. Contents....");
    //** for(int i=0; i<10; i++)
    //** {
    //**     printf("%08X ", ctrl.cmd[i]);
    //** }
    //** printf("\n");

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->ModeSense(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length >= 0);
	if (ctrl.length == 0) {
		Log(Log::Warning,
			"Not supported MODE SENSE page $%02X", ctrl.cmd[2]);

		// Failure (Error)
		Error();
		return;
	}

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	START STOP UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdStartStop()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "START STOP UNIT Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->StartStop(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	SEND DIAGNOSTIC
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSendDiag()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "SEND DIAGNOSTIC Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->SendDiag(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	PREVENT/ALLOW MEDIUM REMOVAL
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdRemoval()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "PREVENT/ALLOW MEDIUM REMOVAL Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Removal(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	READ CAPACITY
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdReadCapacity()
{
	DWORD lun;
	int length;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "READ CAPACITY Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	length = ctrl.unit[lun]->ReadCapacity(ctrl.cmd, ctrl.buffer);
	ASSERT(length >= 0);
	if (length <= 0) {
		Error();
		return;
	}

	// Length setting
	ctrl.length = length;

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	READ(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdRead10()
{
	DWORD lun;
	DWORD record;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Receive message if host bridge
	if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'B', 'R')) {
		CmdGetMessage10();
		return;
	}

	// Get record number and block number
	record = ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	record <<= 8;
	record |= ctrl.cmd[4];
	record <<= 8;
	record |= ctrl.cmd[5];
	ctrl.blocks = ctrl.cmd[7];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[8];

#if defined(DISK_LOG)
	Log(Log::Normal, "READ(10) command record=%08X block=%d", record, ctrl.blocks);
#endif	// DISK_LOG

	// Do not process 0 blocks
	if (ctrl.blocks == 0) {
		Status();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->Read(ctrl.buffer, record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	WRITE(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdWrite10()
{
	DWORD lun;
	DWORD record;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Receive message with host bridge
	if (ctrl.unit[lun]->GetID() == MAKEID('S', 'C', 'B', 'R')) {
		CmdSendMessage10();
		return;
	}

	// Get record number and block number
	record = ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	record <<= 8;
	record |= ctrl.cmd[4];
	record <<= 8;
	record |= ctrl.cmd[5];
	ctrl.blocks = ctrl.cmd[7];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[8];

#if defined(DISK_LOG)
	Log(Log::Normal,
		"WRTIE(10) command record=%08X blocks=%d", record, ctrl.blocks);
#endif	// DISK_LOG

	// Do not process 0 blocks
	if (ctrl.blocks == 0) {
		Status();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->WriteCheck(record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	SEEK(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSeek10()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "SEEK(10) Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->Seek(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	VERIFY
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdVerify()
{
	DWORD lun;
	BOOL status;
	DWORD record;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Get record number and block number
	record = ctrl.cmd[2];
	record <<= 8;
	record |= ctrl.cmd[3];
	record <<= 8;
	record |= ctrl.cmd[4];
	record <<= 8;
	record |= ctrl.cmd[5];
	ctrl.blocks = ctrl.cmd[7];
	ctrl.blocks <<= 8;
	ctrl.blocks |= ctrl.cmd[8];

#if defined(DISK_LOG)
	Log(Log::Normal,
		"VERIFY command record=%08X blocks=%d", record, ctrl.blocks);
#endif	// DISK_LOG

	// Do not process 0 blocks
	if (ctrl.blocks == 0) {
		Status();
		return;
	}

	// if BytChk=0
	if ((ctrl.cmd[1] & 0x02) == 0) {
		// Command processing on drive
		status = ctrl.unit[lun]->Seek(ctrl.cmd);
		if (!status) {
			// Failure (Error)
			Error();
			return;
		}

		// status phase
		Status();
		return;
	}

	// Test loading
	ctrl.length = ctrl.unit[lun]->Read(ctrl.buffer, record);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.next = record + 1;

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	SYNCHRONIZE CACHE
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSynchronizeCache()
{
	DWORD lun;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Make it do something (not implemented)...

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	READ DEFECT DATA(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdReadDefectData10()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "READ DEFECT DATA(10) Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->ReadDefectData10(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length >= 0);

	if (ctrl.length <= 4) {
		Error();
		return;
	}

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdReadToc()
{
	DWORD lun;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->ReadToc(ctrl.cmd, ctrl.buffer);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdPlayAudio10()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->PlayAudio(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO MSF
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdPlayAudioMSF()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->PlayAudioMSF(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO TRACK
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdPlayAudioTrack()
{
	DWORD lun;
	BOOL status;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	status = ctrl.unit[lun]->PlayAudioTrack(ctrl.cmd);
	if (!status) {
		// Failure (Error)
		Error();
		return;
	}

	// status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	MODE SELECT10
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdModeSelect10()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "MODE SELECT10 Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->SelectCheck10(ctrl.cmd);
	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	MODE SENSE(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdModeSense10()
{
	DWORD lun;

	ASSERT(this);

#if defined(DISK_LOG)
	Log(Log::Normal, "MODE SENSE(10) Command ");
#endif	// DISK_LOG

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Command processing on drive
	ctrl.length = ctrl.unit[lun]->ModeSense10(ctrl.cmd, ctrl.buffer);
	ASSERT(ctrl.length >= 0);
	if (ctrl.length == 0) {
		Log(Log::Warning,
			"Not supported MODE SENSE(10) page $%02X", ctrl.cmd[2]);

		// Failure (Error)
		Error();
		return;
	}

	// Data-in Phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	GET MESSAGE(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdGetMessage10()
{
	DWORD lun;
	SCSIBR *bridge;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Error if not a host bridge
	if (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'B', 'R')) {
		Error();
		return;
	}

	// Reallocate buffer (because it is not transfer for each block)
	if (ctrl.bufsize < 0x1000000) {
		free(ctrl.buffer);
		ctrl.bufsize = 0x1000000;
		ctrl.buffer = (BYTE *)malloc(ctrl.bufsize);
	}

	// Process with drive
	bridge = (SCSIBR*)ctrl.unit[lun];
	ctrl.length = bridge->GetMessage10(ctrl.cmd, ctrl.buffer);

	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.blocks = 1;
	ctrl.next = 1;

	// Data in phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	SEND MESSAGE(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::CmdSendMessage10()
{
	DWORD lun;

	ASSERT(this);

	// Logical Unit
	lun = (ctrl.cmd[1] >> 5) & 0x07;
	if (!ctrl.unit[lun]) {
		Error();
		return;
	}

	// Error if not a host bridge
	if (ctrl.unit[lun]->GetID() != MAKEID('S', 'C', 'B', 'R')) {
		Error();
		return;
	}

	// Reallocate buffer (because it is not transfer for each block)
	if (ctrl.bufsize < 0x1000000) {
		free(ctrl.buffer);
		ctrl.bufsize = 0x1000000;
		ctrl.buffer = (BYTE *)malloc(ctrl.bufsize);
	}

	// Set transfer amount
	ctrl.length = ctrl.cmd[6];
	ctrl.length <<= 8;
	ctrl.length |= ctrl.cmd[7];
	ctrl.length <<= 8;
	ctrl.length |= ctrl.cmd[8];

	if (ctrl.length <= 0) {
		// Failure (Error)
		Error();
		return;
	}

	// Set next block
	ctrl.blocks = 1;
	ctrl.next = 1;

	// Light phase
	DataOut();
}

//===========================================================================
//
//	Data Transfer
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Send data
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Send()
{
#ifdef RASCSI
	int len;
#endif	// RASCSI
	BOOL result;

	ASSERT(this);
	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(ctrl.bus->GetIO());

#ifdef RASCSI
	//if Length! = 0, send
	if (ctrl.length != 0) {
		len = ctrl.bus->SendHandShake(
			&ctrl.buffer[ctrl.offset], ctrl.length);

		// If you cannot send all, move to status phase
		if (len != (int)ctrl.length) {
			Error();
			return;
		}

		// offset and length
		ctrl.offset += ctrl.length;
		ctrl.length = 0;
		return;
	}
#else
	// offset and length
	ASSERT(ctrl.length >= 1);
	ctrl.offset++;
	ctrl.length--;

	// Immediately after ACK is asserted, if the data has been
    // set by SendNext, raise the request
	if (ctrl.length != 0) {
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
		return;
	}
#endif	// RASCSI

	// Block subtraction, result initialization
	ctrl.blocks--;
	result = TRUE;

	// Processing after data collection (read/data-in only)
	if (ctrl.phase == BUS::datain) {
		if (ctrl.blocks != 0) {
			// // set next buffer (set offset, length)
			result = XferIn(ctrl.buffer);
#ifndef RASCSI
			ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset]);
#endif	// RASCSI
		}
	}

	// If result FALSE, move to status phase
	if (!result) {
		Error();
		return;
	}

	// Continue sending if block !=0
	if (ctrl.blocks != 0){
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.offset == 0);
#ifndef RASCSI
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

	// Move to next phase
	switch (ctrl.phase) {
		// Message in phase
		case BUS::msgin:
			// Completed sending response to extended message of IDENTIFY message
			if (scsi.atnmsg) {
				// flag off
				scsi.atnmsg = FALSE;

				// command phase
				Command();
			} else {
				// Bus free phase
				BusFree();
			}
			break;

		// Data-in Phase
		case BUS::datain:
			// status phase
			Status();
			break;

		// status phase
		case BUS::status:
			// Message in phase
			ctrl.length = 1;
			ctrl.blocks = 1;
			ctrl.buffer[0] = (BYTE)ctrl.message;
			MsgIn();
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Continue data transmission.....
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::SendNext()
{
	ASSERT(this);

	// REQ is up
	ASSERT(ctrl.bus->GetREQ());
	ASSERT(ctrl.bus->GetIO());

	// Signal line operated by the target
	ctrl.bus->SetREQ(FALSE);

	// If there is data in the buffer, set it first
	if (ctrl.length > 1) {
		ctrl.bus->SetDAT(ctrl.buffer[ctrl.offset + 1]);
	}
}
#endif	// RASCSI

#ifndef RASCSI
//---------------------------------------------------------------------------
//
//	Receive data
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Receive()
{
	DWORD data;

	ASSERT(this);

	// Req is up
	ASSERT(ctrl.bus->GetREQ());
	ASSERT(!ctrl.bus->GetIO());

	// Get data
	data = (DWORD)ctrl.bus->GetDAT();

	// Signal line operated by the target
	ctrl.bus->SetREQ(FALSE);

	switch (ctrl.phase) {
		// Command phase
		case BUS::command:
			ctrl.cmd[ctrl.offset] = data;
#if defined(DISK_LOG)
			Log(Log::Normal, "Command phase $%02X", data);
#endif	// DISK_LOG

			// Set the length again with the first data (offset 0)
			if (ctrl.offset == 0) {
				if (ctrl.cmd[0] >= 0x20) {
					// 10バイトCDB
					ctrl.length = 10;
				}
			}
			break;

		// Message out phase
		case BUS::msgout:
			ctrl.message = data;
#if defined(DISK_LOG)
			Log(Log::Normal, "Message out phase $%02X", data);
#endif	// DISK_LOG
			break;

		// Data out phase
		case BUS::dataout:
			ctrl.buffer[ctrl.offset] = (BYTE)data;
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}
#endif	// RASCSI

#ifdef RASCSI
//---------------------------------------------------------------------------
//
//  Receive Data
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::Receive()
#else
//---------------------------------------------------------------------------
//
//	Continue receiving data
//
//---------------------------------------------------------------------------
void FASTCALL SCSIDEV::ReceiveNext()
#endif	// RASCSI
{
#ifdef RASCSI
	int len;
#endif	// RASCSI
	BOOL result;
	int i;
	BYTE data;

	ASSERT(this);

	// REQ is low
	ASSERT(!ctrl.bus->GetREQ());
	ASSERT(!ctrl.bus->GetIO());

#ifdef RASCSI
	// Length != 0 if received
	if (ctrl.length != 0) {
		// Receive
		len = ctrl.bus->ReceiveHandShake(
			&ctrl.buffer[ctrl.offset], ctrl.length);

		// If not able to receive all, move to status phase
		if (len != (int)ctrl.length) {
			Error();
			return;
		}

		// Offset and Length
		ctrl.offset += ctrl.length;
		ctrl.length = 0;;
		return;
	}
#else
	// Offset and Length
	ASSERT(ctrl.length >= 1);
	ctrl.offset++;
	ctrl.length--;

	// If length!=0, set req again
	if (ctrl.length != 0) {
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
		return;
	}
#endif	// RASCSI

	// Block subtraction, result initialization
	ctrl.blocks--;
	result = TRUE;

	// Processing after receiving data (by phase)
	switch (ctrl.phase) {

		// Data out phase
		case BUS::dataout:
			if (ctrl.blocks == 0) {
				// End with this buffer
				result = XferOut(FALSE);
			} else {
				// Continue to next buffer (set offset, length)
				result = XferOut(TRUE);
			}
			break;

		// Message out phase
		case BUS::msgout:
			ctrl.message = ctrl.buffer[0];
			if (!XferMsg(ctrl.message)) {
				// Immediately free the bus if message output fails
				BusFree();
				return;
			}

			// Clear message data in preparation for message-in
			ctrl.message = 0x00;
			break;

		default:
			break;
	}

	// If result FALSE, move to status phase
	if (!result) {
		Error();
		return;
	}

	// Continue to receive if block !=0
	if (ctrl.blocks != 0){
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.offset == 0);
#ifndef RASCSI
		// Signal line operated by the target
		ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
		return;
	}

	// Move to next phase
	switch (ctrl.phase) {
		// Command phase
		case BUS::command:
#ifdef RASCSI
			// Command data transfer
			len = 6;
			if (ctrl.buffer[0] >= 0x20 && ctrl.buffer[0] <= 0x7D) {
				// 10 byte CDB
				len = 10;
			}
			for (i = 0; i < len; i++) {
				ctrl.cmd[i] = (DWORD)ctrl.buffer[i];
#if defined(DISK_LOG)
				Log(Log::Normal, "Command $%02X", ctrl.cmd[i]);
#endif	// DISK_LOG
			}
#endif	// RASCSI

			// Execution Phase
			Execute();
			break;

		// Message out phase
		case BUS::msgout:
			// Continue message out phase as long as ATN keeps asserting
			if (ctrl.bus->GetATN()) {
				// Data transfer is 1 byte x 1 block
				ctrl.offset = 0;
				ctrl.length = 1;
				ctrl.blocks = 1;
#ifndef RASCSI
				// Request message
				ctrl.bus->SetREQ(TRUE);
#endif	// RASCSI
				return;
			}

			// Parsing messages sent by ATN
			if (scsi.atnmsg) {
				i = 0;
				while (i < scsi.msc) {
					// Message type
					data = scsi.msb[i];

					// ABORT
					if (data == 0x06) {
#if defined(DISK_LOG)
						Log(Log::Normal,
							"Message code ABORT $%02X", data);
#endif	// DISK_LOG
						BusFree();
						return;
					}

					// BUS DEVICE RESET
					if (data == 0x0C) {
#if defined(DISK_LOG)
						Log(Log::Normal,
							"Message code BUS DEVICE RESET $%02X", data);
#endif	// DISK_LOG
						scsi.syncoffset = 0;
						BusFree();
						return;
					}

					// IDENTIFY
					if (data >= 0x80) {
#if defined(DISK_LOG)
						Log(Log::Normal,
							"Message code IDENTIFY $%02X", data);
#endif	// DISK_LOG
					}

					// Extended Message
					if (data == 0x01) {
#if defined(DISK_LOG)
						Log(Log::Normal,
							"Message code EXTENDED MESSAGE $%02X", data);
#endif	// DISK_LOG

						// Check only when synchronous transfer is possible
						if (!scsi.syncenable || scsi.msb[i + 2] != 0x01) {
							ctrl.length = 1;
							ctrl.blocks = 1;
							ctrl.buffer[0] = 0x07;
							MsgIn();
							return;
						}

						// Transfer period factor (limited to 50 x 4 = 200ns)
						scsi.syncperiod = scsi.msb[i + 3];
						if (scsi.syncperiod > 50) {
							scsi.syncoffset = 50;
						}

						// REQ/ACK offset(limited to 16)
						scsi.syncoffset = scsi.msb[i + 4];
						if (scsi.syncoffset > 16) {
							scsi.syncoffset = 16;
						}

						// STDR response message generation
						ctrl.length = 5;
						ctrl.blocks = 1;
						ctrl.buffer[0] = 0x01;
						ctrl.buffer[1] = 0x03;
						ctrl.buffer[2] = 0x01;
						ctrl.buffer[3] = (BYTE)scsi.syncperiod;
						ctrl.buffer[4] = (BYTE)scsi.syncoffset;
						MsgIn();
						return;
					}

					// next
					i++;
				}
			}

			// Initialize ATN message reception status
			scsi.atnmsg = FALSE;

			// Command phase
			Command();
			break;

		// Data out phase
		case BUS::dataout:
			// Flush unit
			FlushUnit();

			// status phase
			Status();
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Transfer MSG
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIDEV::XferMsg(DWORD msg)
{
	ASSERT(this);
	ASSERT(ctrl.phase == BUS::msgout);

	// Save message out data
	if (scsi.atnmsg) {
		scsi.msb[scsi.msc] = (BYTE)msg;
		scsi.msc++;
		scsi.msc %= 256;
	}

	return TRUE;
}
