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
//  	Comments translated to english by akuker.
//
//---------------------------------------------------------------------------

#include "os.h"
#include "device_factory.h"
#include "exceptions.h"
#include "disk.h"
#include "mode_page_device.h"
#include "disk_image/disk_image_handle_factory.h"

using namespace scsi_defs;

Disk::Disk(const string& id) : ModePageDevice(id), ScsiBlockCommands()
{
	// Work initialization
	configured_sector_size = 0;
	disk.size = 0;
	disk.blocks = 0;
	disk.dcache = NULL;
	disk.image_offset = 0;
	disk.is_medium_changed = false;

	dispatcher.AddCommand(eCmdRezero, "Rezero", &Disk::Rezero);
	dispatcher.AddCommand(eCmdFormat, "FormatUnit", &Disk::FormatUnit);
	dispatcher.AddCommand(eCmdReassign, "ReassignBlocks", &Disk::ReassignBlocks);
	dispatcher.AddCommand(eCmdRead6, "Read6", &Disk::Read6);
	dispatcher.AddCommand(eCmdWrite6, "Write6", &Disk::Write6);
	dispatcher.AddCommand(eCmdSeek6, "Seek6", &Disk::Seek6);
	dispatcher.AddCommand(eCmdReserve6, "Reserve6", &Disk::Reserve);
	dispatcher.AddCommand(eCmdRelease6, "Release6", &Disk::Release);
	dispatcher.AddCommand(eCmdStartStop, "StartStopUnit", &Disk::StartStopUnit);
	dispatcher.AddCommand(eCmdSendDiag, "SendDiagnostic", &Disk::SendDiagnostic);
	dispatcher.AddCommand(eCmdRemoval, "PreventAllowMediumRemoval", &Disk::PreventAllowMediumRemoval);
	dispatcher.AddCommand(eCmdReadCapacity10, "ReadCapacity10", &Disk::ReadCapacity10);
	dispatcher.AddCommand(eCmdRead10, "Read10", &Disk::Read10);
	dispatcher.AddCommand(eCmdWrite10, "Write10", &Disk::Write10);
	dispatcher.AddCommand(eCmdReadLong10, "ReadLong10", &Disk::ReadLong10);
	dispatcher.AddCommand(eCmdWriteLong10, "WriteLong10", &Disk::WriteLong10);
	dispatcher.AddCommand(eCmdWriteLong16, "WriteLong16", &Disk::WriteLong16);
	dispatcher.AddCommand(eCmdSeek10, "Seek10", &Disk::Seek10);
	dispatcher.AddCommand(eCmdVerify10, "Verify10", &Disk::Verify10);
	dispatcher.AddCommand(eCmdSynchronizeCache10, "SynchronizeCache10", &Disk::SynchronizeCache10);
	dispatcher.AddCommand(eCmdSynchronizeCache16, "SynchronizeCache16", &Disk::SynchronizeCache16);
	dispatcher.AddCommand(eCmdReadDefectData10, "ReadDefectData10", &Disk::ReadDefectData10);
	dispatcher.AddCommand(eCmdReserve10, "Reserve10", &Disk::Reserve);
	dispatcher.AddCommand(eCmdRelease10, "Release10", &Disk::Release);
	dispatcher.AddCommand(eCmdRead16, "Read16", &Disk::Read16);
	dispatcher.AddCommand(eCmdWrite16, "Write16", &Disk::Write16);
	dispatcher.AddCommand(eCmdVerify16, "Verify16", &Disk::Verify16);
	dispatcher.AddCommand(eCmdReadCapacity16_ReadLong16, "ReadCapacity16/ReadLong16", &Disk::ReadCapacity16_ReadLong16);
}

Disk::~Disk()
{
	// Save disk cache
	if (IsReady()) {
		// Only if ready...
		FlushCache();
	}

	// Clear disk cache
	if (disk.dcache) {
		delete disk.dcache;
		disk.dcache = NULL;
	}
}

bool Disk::Dispatch(SCSIDEV *controller)
{
	// Media changes must be reported on the next access, i.e. not only for TEST UNIT READY
	if (disk.is_medium_changed) {
		assert(IsRemovable());

		disk.is_medium_changed = false;

		controller->Error(ERROR_CODES::sense_key::UNIT_ATTENTION, ERROR_CODES::asc::NOT_READY_TO_READY_CHANGE);
		return true;
	}

	// The superclass handles the less specific commands
	return dispatcher.Dispatch(this, controller) ? true : super::Dispatch(controller);
}

//---------------------------------------------------------------------------
//
//	Open
//  * Call as a post-process after successful opening in a derived class
//
//---------------------------------------------------------------------------
void Disk::Open(const Filepath& path)
{
	assert(disk.blocks > 0);

	SetReady(true);

	// Cache initialization
	assert (!disk.dcache);
	disk.dcache = DiskImageHandleFactory::CreateDiskImageHandle(path, disk.size, disk.blocks, disk.image_offset);

	// Can read/write open
	Fileio fio;
	if (fio.Open(path, Fileio::ReadWrite)) {
		// Write permission
		fio.Close();
	} else {
		// Permanently write-protected
		SetReadOnly(true);
		SetProtectable(false);
		SetProtected(false);
	}

	SetStopped(false);
	SetRemoved(false);
	SetLocked(false);
}

void Disk::FlushCache()
{
	if (disk.dcache) {
		disk.dcache->Save();
	}
}

void Disk::Rezero(SASIDEV *controller)
{
	if (!CheckReady()) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::FormatUnit(SASIDEV *controller)
{
	if (!Format(ctrl->cmd)) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::ReassignBlocks(SASIDEV *controller)
{
	if (!CheckReady()) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::Read(SASIDEV *controller, uint64_t record)
{
	ctrl->length = Read(ctrl->cmd, ctrl->buffer, record);
	LOGTRACE("%s ctrl.length is %d", __PRETTY_FUNCTION__, (int)ctrl->length);

	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	// Set next block
	ctrl->next = record + 1;

	controller->DataIn();
}

void Disk::Read6(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW6)) {
		Read(controller, start);
	}
}

void Disk::Read10(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW10)) {
		Read(controller, start);
	}
}

void Disk::Read16(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW16)) {
		Read(controller, start);
	}
}

void Disk::ReadWriteLong10(SASIDEV *controller)
{
	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (ctrl->cmd[7] || ctrl->cmd[8]) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		return;
	}

	if (CheckBlockAddress(controller, RW10)) {
		controller->Status();
	}
}

void Disk::ReadLong10(SASIDEV *controller)
{
	ReadWriteLong10(controller);
}

void Disk::ReadWriteLong16(SASIDEV *controller)
{
	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (ctrl->cmd[12] || ctrl->cmd[13]) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		return;
	}

	if (CheckBlockAddress(controller, RW16)) {
		controller->Status();
	}
}

void Disk::ReadLong16(SASIDEV *controller)
{
	ReadWriteLong16(controller);
}

void Disk::Write(SASIDEV *controller, uint64_t record)
{
	ctrl->length = WriteCheck(record);
	if (ctrl->length == 0) {
		controller->Error(ERROR_CODES::sense_key::NOT_READY, ERROR_CODES::asc::NO_ADDITIONAL_SENSE_INFORMATION);
		return;
	}
	else if (ctrl->length < 0) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::WRITE_PROTECTED);
		return;
	}

	// Set next block
	ctrl->next = record + 1;

	controller->DataOut();
}

void Disk::Write6(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW6)) {
		Write(controller, start);
	}
}

void Disk::Write10(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW10)) {
		Write(controller, start);
	}
}

void Disk::Write16(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW16)) {
		Write(controller, start);
	}
}

void Disk::WriteLong10(SASIDEV *controller)
{
	ReadWriteLong10(controller);
}

void Disk::WriteLong16(SASIDEV *controller)
{
	ReadWriteLong16(controller);
}

void Disk::Verify(SASIDEV *controller, uint64_t record)
{
	// if BytChk=0
	if ((ctrl->cmd[1] & 0x02) == 0) {
		Seek(controller);
		return;
	}

	// Test loading
	ctrl->length = Read(ctrl->cmd, ctrl->buffer, record);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	// Set next block
	ctrl->next = record + 1;

	controller->DataOut();
}

void Disk::Verify10(SASIDEV *controller)
{
	// Get record number and block number
	uint64_t record;
	if (GetStartAndCount(controller, record, ctrl->blocks, RW10)) {
		Verify(controller, record);
	}
}

void Disk::Verify16(SASIDEV *controller)
{
	// Get record number and block number
	uint64_t record;
	if (GetStartAndCount(controller, record, ctrl->blocks, RW16)) {
		Verify(controller, record);
	}
}

void Disk::StartStopUnit(SASIDEV *controller)
{
	if (!StartStop(ctrl->cmd)) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::SendDiagnostic(SASIDEV *controller)
{
	if (!SendDiag(ctrl->cmd)) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::PreventAllowMediumRemoval(SASIDEV *controller)
{
	if (!CheckReady()) {
		controller->Error();
		return;
	}

	bool lock = ctrl->cmd[4] & 0x01;

	LOGTRACE("%s", lock ? "Locking medium" : "Unlocking medium");

	SetLocked(lock);

	controller->Status();
}

void Disk::SynchronizeCache10(SASIDEV *controller)
{
	FlushCache();

	controller->Status();
}

void Disk::SynchronizeCache16(SASIDEV *controller)
{
	return SynchronizeCache10(controller);
}

void Disk::ReadDefectData10(SASIDEV *controller)
{
	ctrl->length = ReadDefectData10(ctrl->cmd, ctrl->buffer, ctrl->bufsize);
	if (ctrl->length <= 4) {
		controller->Error();
		return;
	}

	controller->DataIn();
}

void Disk::MediumChanged()
{
	assert(IsRemovable());

	if (IsRemovable()) {
		disk.is_medium_changed = true;
	}
}

bool Disk::Eject(bool force)
{
	bool status = Device::Eject(force);
	if (status) {
		FlushCache();
		delete disk.dcache;
		disk.dcache = NULL;

		// The image file for this drive is not in use anymore
		FileSupport *file_support = dynamic_cast<FileSupport *>(this);
		if (file_support) {
			file_support->UnreserveFile();
		}
	}

	return status;
}

int Disk::ModeSense6(const DWORD *cdb, BYTE *buf)
{
	// Get length, clear buffer
	int length = (int)cdb[4];
	memset(buf, 0, length);

	// Basic information
	int size = 4;

	// Add block descriptor if DBD is 0
	if ((cdb[1] & 0x08) == 0) {
		// Mode parameter header, block descriptor length
		buf[3] = 0x08;

		// Only if ready
		if (IsReady()) {
			// Short LBA mode parameter block descriptor (number of blocks and block length)

			uint64_t disk_blocks = GetBlockCount();
			buf[4] = disk_blocks >> 24;
			buf[5] = disk_blocks >> 16;
			buf[6] = disk_blocks >> 8;
			buf[7] = disk_blocks;

			// Block descriptor (block length)
			uint32_t disk_size = GetSectorSizeInBytes();
			buf[9] = disk_size >> 16;
			buf[10] = disk_size >> 8;
			buf[11] = disk_size;
		}

		size = 12;
	}

	int pages_size = super::AddModePages(cdb, &buf[size], length - size);
	if (!pages_size) {
		return 0;
	}
	size += pages_size;

	// Do not return more than ALLOCATION LENGTH bytes
	if (size > length) {
		size = length;
	}

	// Final setting of mode data length
	buf[0] = size;

	return size;
}

int Disk::ModeSense10(const DWORD *cdb, BYTE *buf, int max_length)
{
	// Get length, clear buffer
	int length = (cdb[7] << 8) | cdb[8];
	if (length > max_length) {
		length = max_length;
	}
	memset(buf, 0, length);

	// Basic Information
	int size = 8;

	// Add block descriptor if DBD is 0
	if ((cdb[1] & 0x08) == 0) {
		// Only if ready
		if (IsReady()) {
			uint64_t disk_blocks = GetBlockCount();
			uint32_t disk_size = GetSectorSizeInBytes();

			// Check LLBAA for short or long block descriptor
			if ((cdb[1] & 0x10) == 0 || disk_blocks <= 0xFFFFFFFF) {
				// Mode parameter header, block descriptor length
				buf[7] = 0x08;

				// Short LBA mode parameter block descriptor (number of blocks and block length)

				buf[8] = disk_blocks >> 24;
				buf[9] = disk_blocks >> 16;
				buf[10] = disk_blocks >> 8;
				buf[11] = disk_blocks;

				buf[13] = disk_size >> 16;
				buf[14] = disk_size >> 8;
				buf[15] = disk_size;

				size = 16;
			}
			else {
				// Mode parameter header, LONGLBA
				buf[4] = 0x01;

				// Mode parameter header, block descriptor length
				buf[7] = 0x10;

				// Long LBA mode parameter block descriptor (number of blocks and block length)

				buf[8] = disk_blocks >> 56;
				buf[9] = disk_blocks >> 48;
				buf[10] = disk_blocks >> 40;
				buf[11] = disk_blocks >> 32;
				buf[12] = disk_blocks >> 24;
				buf[13] = disk_blocks >> 16;
				buf[14] = disk_blocks >> 8;
				buf[15] = disk_blocks;

				buf[20] = disk_size >> 24;
				buf[21] = disk_size >> 16;
				buf[22] = disk_size >> 8;
				buf[23] = disk_size;

				size = 24;
			}
		}
	}

	int pages_size = super::AddModePages(cdb, &buf[size], length - size);
	if (!pages_size) {
		return 0;
	}
	size += pages_size;

	// Do not return more than ALLOCATION LENGTH bytes
	if (size > length) {
		size = length;
	}

	// Final setting of mode data length
	buf[0] = size >> 8;
	buf[1] = size;

	return size;
}

void Disk::SetDeviceParameters(BYTE *buf)
{
	// DEVICE SPECIFIC PARAMETER
	if (IsProtected()) {
		buf[3] = 0x80;
	}
}

void Disk::AddModePages(map<int, vector<BYTE>>& pages, int page, bool changeable) const
{
	// Page code 1 (read-write error recovery)
	if (page == 0x01 || page == 0x3f) {
		AddErrorPage(pages, changeable);
	}

	// Page code 3 (format device)
	if (page == 0x03 || page == 0x3f) {
		AddFormatPage(pages, changeable);
	}

	// Page code 4 (drive parameter)
	if (page == 0x04 || page == 0x3f) {
		AddDrivePage(pages, changeable);
	}

	// Page code 8 (caching)
	if (page == 0x08 || page == 0x3f) {
		AddCachePage(pages, changeable);
	}

	// Page (vendor special)
	AddVendorPage(pages, page, changeable);
}

void Disk::AddErrorPage(map<int, vector<BYTE>>& pages, bool) const
{
	vector<BYTE> buf(12);

	// Retry count is 0, limit time uses internal default value

	pages[1] = buf;
}

void Disk::AddFormatPage(map<int, vector<BYTE>>& pages, bool changeable) const
{
	vector<BYTE> buf(24);

	// Page can be saved
	buf[0] = 0x80;

	// Show the number of bytes in the physical sector as changeable
	// (though it cannot be changed in practice)
	if (changeable) {
		buf[0xc] = 0xff;
		buf[0xd] = 0xff;

		pages[3] = buf;

		return;
	}

	if (IsReady()) {
		// Set the number of tracks in one zone to 8 (TODO)
		buf[0x3] = 0x08;

		// Set sector/track to 25 (TODO)
		buf[0xa] = 0x00;
		buf[0xb] = 0x19;

		// Set the number of bytes in the physical sector
		int size = 1 << disk.size;
		buf[0xc] = (BYTE)(size >> 8);
		buf[0xd] = (BYTE)size;
	}

	// Set removable attribute
	if (IsRemovable()) {
		buf[20] = 0x20;
	}

	pages[3] = buf;
}

void Disk::AddDrivePage(map<int, vector<BYTE>>& pages, bool changeable) const
{
	vector<BYTE> buf(24);

	// No changeable area
	if (changeable) {
		pages[4] = buf;

		return;
	}

	if (IsReady()) {
		// Set the number of cylinders (total number of blocks
        // divided by 25 sectors/track and 8 heads)
		uint32_t cylinder = disk.blocks;
		cylinder >>= 3;
		cylinder /= 25;
		buf[0x2] = (BYTE)(cylinder >> 16);
		buf[0x3] = (BYTE)(cylinder >> 8);
		buf[0x4] = (BYTE)cylinder;

		// Fix the head at 8
		buf[0x5] = 0x8;
	}

	pages[4] = buf;
}

void Disk::AddCachePage(map<int, vector<BYTE>>& pages, bool) const
{
	vector<BYTE> buf(12);

	// Only read cache is valid, no prefetch

	pages[8] = buf;
}

void Disk::AddVendorPage(map<int, vector<BYTE>>&, int, bool) const
{
	// Nothing to add by default
}

int Disk::ReadDefectData10(const DWORD *cdb, BYTE *buf, int max_length)
{
	// Get length, clear buffer
	int length = (cdb[7] << 8) | cdb[8];
	if (length > max_length) {
		length = max_length;
	}
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
	SetStatusCode(STATUS_NODEFECT);
	return 4;
}

//---------------------------------------------------------------------------
//
//	FORMAT UNIT
//	*Opcode $06 for SASI, Opcode $04 for SCSI
//
//---------------------------------------------------------------------------
bool Disk::Format(const DWORD *cdb)
{
	if (!CheckReady()) {
		return false;
	}

	// FMTDATA=1 is not supported (but OK if there is no DEFECT LIST)
	if ((cdb[1] & 0x10) != 0 && cdb[4] != 0) {
		SetStatusCode(STATUS_INVALIDCDB);
		return false;
	}

	// FORMAT Success
	return true;
}

// TODO Read more than one block in a single call. Currently blocked by the SASI code (missing early range check)
// and the track-oriented cache.
int Disk::Read(const DWORD *cdb, BYTE *buf, uint64_t block)
{
	LOGTRACE("%s", __PRETTY_FUNCTION__);

	if (!CheckReady()) {
		return 0;
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		SetStatusCode(STATUS_INVALIDLBA);
		return 0;
	}

	// leave it to the cache
	if (!disk.dcache->ReadSector(buf, block)) {
		SetStatusCode(STATUS_READFAULT);
		return 0;
	}

	//  Success
	return 1 << disk.size;
}

int Disk::WriteCheck(DWORD block)
{
	// Status check
	if (!CheckReady()) {
		LOGDEBUG("WriteCheck failed (not ready)");
		return 0;
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		LOGDEBUG("WriteCheck failed (capacity exceeded)");
		return 0;
	}

	// Error if write protected
	if (IsProtected()) {
		LOGDEBUG("WriteCheck failed (protected)");
		return -1;
	}

	//  Success
	return 1 << disk.size;
}

// TODO Write more than one block in a single call. Currently blocked by the SASI code (missing early range check)
// and the track-oriented cache.
bool Disk::Write(const DWORD *cdb, const BYTE *buf, DWORD block)
{
	LOGTRACE("%s", __PRETTY_FUNCTION__);

	// Error if not ready
	if (!IsReady()) {
		SetStatusCode(STATUS_NOTREADY);
		return false;
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		SetStatusCode(STATUS_INVALIDLBA);
		return false;
	}

	// Error if write protected
	if (IsProtected()) {
		SetStatusCode(STATUS_WRITEPROTECT);
		return false;
	}

	// Leave it to the cache
	if (!disk.dcache->WriteSector(buf, block)) {
		SetStatusCode(STATUS_WRITEFAULT);
		return false;
	}

	return true;
}

// TODO For SCSI the specification mandates that the block address is verified
void Disk::Seek(SASIDEV *controller)
{
	if (!CheckReady()) {
		controller->Error();
		return;
	}

	controller->Status();
}

//---------------------------------------------------------------------------
//
//	SEEK(6)
//	Does not check LBA (SASI IOCS)
//
//---------------------------------------------------------------------------
void Disk::Seek6(SASIDEV *controller)
{
	Seek(controller);
}

void Disk::Seek10(SASIDEV *controller)
{
	Seek(controller);
}

bool Disk::StartStop(const DWORD *cdb)
{
	bool start = cdb[4] & 0x01;
	bool load = cdb[4] & 0x02;

	if (load) {
		LOGTRACE("%s", start ? "Loading medium" : "Ejecting medium");
	}
	else {
		LOGTRACE("%s", start ? "Starting unit" : "Stopping unit");

		SetStopped(!start);
	}

	if (!start) {
		FlushCache();

		// Look at the eject bit and eject if necessary
		if (load) {
			if (IsLocked()) {
				// Cannot be ejected because it is locked
				SetStatusCode(STATUS_PREVENT);
				return false;
			}

			// Eject
			return Eject(false);
		}
	}

	return true;
}

bool Disk::SendDiag(const DWORD *cdb)
{
	// Do not support PF bit
	if (cdb[1] & 0x10) {
		SetStatusCode(STATUS_INVALIDCDB);
		return false;
	}

	// Do not support parameter list
	if ((cdb[3] != 0) || (cdb[4] != 0)) {
		SetStatusCode(STATUS_INVALIDCDB);
		return false;
	}

	return true;
}

void Disk::ReadCapacity10(SASIDEV *controller)
{
	if (!CheckReady() || disk.blocks <= 0) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::MEDIUM_NOT_PRESENT);
		return;
	}

	BYTE *buf = ctrl->buffer;

	// Create end of logical block address (disk.blocks-1)
	uint32_t blocks = disk.blocks - 1;
	buf[0] = (BYTE)(blocks >> 24);
	buf[1] = (BYTE)(blocks >> 16);
	buf[2] = (BYTE)(blocks >> 8);
	buf[3] = (BYTE)blocks;

	// Create block length (1 << disk.size)
	uint32_t length = 1 << disk.size;
	buf[4] = (BYTE)(length >> 24);
	buf[5] = (BYTE)(length >> 16);
	buf[6] = (BYTE)(length >> 8);
	buf[7] = (BYTE)length;

	// the size
	ctrl->length = 8;

	controller->DataIn();
}

void Disk::ReadCapacity16(SASIDEV *controller)
{
	if (!CheckReady() || disk.blocks <= 0) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::MEDIUM_NOT_PRESENT);
		return;
	}

	BYTE *buf = ctrl->buffer;

	// Create end of logical block address (disk.blocks-1)
	uint64_t blocks = disk.blocks - 1;
	buf[0] = (BYTE)(blocks >> 56);
	buf[1] = (BYTE)(blocks >> 48);
	buf[2] = (BYTE)(blocks >> 40);
	buf[3] = (BYTE)(blocks >> 32);
	buf[4] = (BYTE)(blocks >> 24);
	buf[5] = (BYTE)(blocks >> 16);
	buf[6] = (BYTE)(blocks >> 8);
	buf[7] = (BYTE)blocks;

	// Create block length (1 << disk.size)
	uint32_t length = 1 << disk.size;
	buf[8] = (BYTE)(length >> 24);
	buf[9] = (BYTE)(length >> 16);
	buf[10] = (BYTE)(length >> 8);
	buf[11] = (BYTE)length;

	buf[12] = 0;

	// Logical blocks per physical block: not reported (1 or more)
	buf[13] = 0;

	// the size
	ctrl->length = 14;

	controller->DataIn();
}

void Disk::ReadCapacity16_ReadLong16(SASIDEV *controller)
{
	// The service action determines the actual command
	switch (ctrl->cmd[1] & 0x1f) {
	case 0x10:
		ReadCapacity16(controller);
		break;

	case 0x11:
		ReadLong16(controller);
		break;

	default:
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		break;
	}
}

//---------------------------------------------------------------------------
//
//	RESERVE/RELEASE(6/10)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void Disk::Reserve(SASIDEV *controller)
{
	controller->Status();
}

void Disk::Release(SASIDEV *controller)
{
	controller->Status();
}

//---------------------------------------------------------------------------
//
//	Check/Get start sector and sector count for a READ/WRITE or READ/WRITE LONG operation
//
//---------------------------------------------------------------------------

bool Disk::CheckBlockAddress(SASIDEV *controller, access_mode mode)
{
	uint64_t block = ctrl->cmd[2];
	block <<= 8;
	block |= ctrl->cmd[3];
	block <<= 8;
	block |= ctrl->cmd[4];
	block <<= 8;
	block |= ctrl->cmd[5];

	if (mode == RW16) {
		block <<= 8;
		block |= ctrl->cmd[6];
		block <<= 8;
		block |= ctrl->cmd[7];
		block <<= 8;
		block |= ctrl->cmd[8];
		block <<= 8;
		block |= ctrl->cmd[9];
	}

	uint64_t capacity = GetBlockCount();
	if (block > capacity) {
		LOGTRACE("%s", ("Capacity of " + to_string(capacity) + " blocks exceeded: Trying to access block "
				+ to_string(block)).c_str());
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::LBA_OUT_OF_RANGE);
		return false;
	}

	return true;
}

bool Disk::GetStartAndCount(SASIDEV *controller, uint64_t& start, uint32_t& count, access_mode mode)
{
	if (mode == RW6) {
		start = ctrl->cmd[1] & 0x1f;
		start <<= 8;
		start |= ctrl->cmd[2];
		start <<= 8;
		start |= ctrl->cmd[3];

		count = ctrl->cmd[4];
		if (!count) {
			count= 0x100;
		}
	}
	else {
		start = ctrl->cmd[2];
		start <<= 8;
		start |= ctrl->cmd[3];
		start <<= 8;
		start |= ctrl->cmd[4];
		start <<= 8;
		start |= ctrl->cmd[5];

		if (mode == RW16) {
			start <<= 8;
			start |= ctrl->cmd[6];
			start <<= 8;
			start |= ctrl->cmd[7];
			start <<= 8;
			start |= ctrl->cmd[8];
			start <<= 8;
			start |= ctrl->cmd[9];
		}

		if (mode == RW16) {
			count = ctrl->cmd[10];
			count <<= 8;
			count |= ctrl->cmd[11];
			count <<= 8;
			count |= ctrl->cmd[12];
			count <<= 8;
			count |= ctrl->cmd[13];
		}
		else {
			count = ctrl->cmd[7];
			count <<= 8;
			count |= ctrl->cmd[8];
		}
	}

	LOGDEBUG("%s READ/WRITE/VERIFY command record=$%08X blocks=%d", __PRETTY_FUNCTION__, (uint32_t)start, count);

	// Check capacity
	uint64_t capacity = GetBlockCount();
	if (start > capacity || start + count > capacity) {
		LOGTRACE("%s", ("Capacity of " + to_string(capacity) + " blocks exceeded: Trying to access block "
				+ to_string(start) + ", block count " + to_string(ctrl->blocks)).c_str());
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::LBA_OUT_OF_RANGE);
		return false;
	}

	// Do not process 0 blocks
	if (!count) {
		LOGTRACE("NOT processing 0 blocks");
		controller->Status();
		return false;
	}

	return true;
}

uint32_t Disk::GetSectorSizeInBytes() const
{
	return disk.size ? 1 << disk.size : 0;
}

void Disk::SetSectorSizeInBytes(uint32_t size, bool sasi)
{
	set<uint32_t> sector_sizes = DeviceFactory::instance().GetSectorSizes(GetType());
	if (!sector_sizes.empty() && sector_sizes.find(size) == sector_sizes.end()) {
		throw io_exception("Invalid block size of " + to_string(size) + " bytes");
	}

	switch (size) {
		case 256:
			disk.size = 8;
			break;

		case 512:
			disk.size = 9;
			break;

		case 1024:
			disk.size = 10;
			break;

		case 2048:
			disk.size = 11;
			break;

		case 4096:
			disk.size = 12;
			break;

		default:
			assert(false);
			break;
	}
}

uint32_t Disk::GetSectorSizeShiftCount() const
{
	return disk.size;
}

void Disk::SetSectorSizeShiftCount(uint32_t size)
{
	disk.size = size;
}

bool Disk::IsSectorSizeConfigurable() const
{
	return !sector_sizes.empty();
}

void Disk::SetSectorSizes(const set<uint32_t>& sector_sizes)
{
	this->sector_sizes = sector_sizes;
}

uint32_t Disk::GetConfiguredSectorSize() const
{
	return configured_sector_size;
}

bool Disk::SetConfiguredSectorSize(uint32_t configured_sector_size)
{
	DeviceFactory& device_factory = DeviceFactory::instance();

	set<uint32_t> sector_sizes = device_factory.GetSectorSizes(GetType());
	if (sector_sizes.find(configured_sector_size) == sector_sizes.end()) {
		return false;
	}

	this->configured_sector_size = configured_sector_size;

	return true;
}

void Disk::SetGeometries(const map<uint64_t, Geometry>& geometries)
{
	this->geometries = geometries;
}

bool Disk::SetGeometryForCapacity(uint64_t capacity) {
	const auto& geometry = geometries.find(capacity);
	if (geometry != geometries.end()) {
		SetSectorSizeInBytes(geometry->second.first, false);
		SetBlockCount(geometry->second.second);

		return true;
	}

	return false;
}

uint64_t Disk::GetBlockCount() const
{
	return disk.blocks;
}

void Disk::SetBlockCount(uint32_t blocks)
{
	disk.blocks = blocks;
}
