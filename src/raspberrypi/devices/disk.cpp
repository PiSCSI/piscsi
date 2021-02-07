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
//	SCSI Check
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::IsSCSI() const
{
	ASSERT(this);
	// If this isn't SASI, then it must be SCSI.
	return (this->IsSASI()) ? FALSE : TRUE;
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

	// Basic information
	size = 4;

	// MEDIUM TYPE
	if (disk.id == MAKEID('S', 'C', 'M', 'O')) {
		buf[1] = 0x03; // optical reversible or erasable
	}

	// DEVICE SPECIFIC PARAMETER
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
		disk.code = DISK_INVALIDCDB;
		return 0;
	}

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

	// Show the number of bytes in the physical sector as changeable
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

