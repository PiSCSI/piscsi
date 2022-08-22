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
#include "fileio.h"
#include "device_factory.h"
#include "exceptions.h"
#include "disk.h"
#include "mode_page_device.h"

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
	dispatcher.AddCommand(eCmdReadLong10, "ReadLong10", &Disk::ReadWriteLong10);
	dispatcher.AddCommand(eCmdWriteLong10, "WriteLong10", &Disk::ReadWriteLong10);
	dispatcher.AddCommand(eCmdWriteLong16, "WriteLong16", &Disk::ReadWriteLong16);
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

bool Disk::Dispatch(Controller *controller)
{
	// Media changes must be reported on the next access, i.e. not only for TEST UNIT READY
	if (disk.is_medium_changed) {
		assert(IsRemovable());

		disk.is_medium_changed = false;

		throw scsi_dispatch_error_exception(sense_key::UNIT_ATTENTION, asc::NOT_READY_TO_READY_CHANGE);
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
	disk.dcache = new DiskCache(path, disk.size, disk.blocks, disk.image_offset);

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

void Disk::Rezero(Controller *controller)
{
	CheckReady();

	controller->Status();
}

void Disk::FormatUnit(Controller *controller)
{
	Format(ctrl->cmd);

	controller->Status();
}

void Disk::ReassignBlocks(Controller *controller)
{
	CheckReady();

	controller->Status();
}

void Disk::Read(Controller *controller, access_mode mode)
{
	uint64_t start;
	if (!GetStartAndCount(controller, start, ctrl->blocks, mode)) {
		return;
	}

	ctrl->length = Read(ctrl->cmd, ctrl->buffer, start);
	LOGTRACE("%s ctrl.length is %d", __PRETTY_FUNCTION__, (int)ctrl->length);

	// Set next block
	ctrl->next = start + 1;

	controller->DataIn();
}

void Disk::Read6(Controller *controller)
{
	Read(controller, RW6);
}

void Disk::Read10(Controller *controller)
{
	Read(controller, RW10);
}

void Disk::Read16(Controller *controller)
{
	Read(controller, RW16);
}

void Disk::ReadWriteLong10(Controller *controller)
{
	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (ctrl->cmd[7] || ctrl->cmd[8]) {
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	ValidateBlockAddress(controller, RW10);

	controller->Status();
}

void Disk::ReadWriteLong16(Controller *controller)
{
	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (ctrl->cmd[12] || ctrl->cmd[13]) {
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
		return;
	}

	ValidateBlockAddress(controller, RW16);

	controller->Status();
}

void Disk::Write(Controller *controller, access_mode mode)
{
	uint64_t start;
	if (!GetStartAndCount(controller, start, ctrl->blocks, mode)) {
		return;
	}

	ctrl->length = WriteCheck(start);

	// Set next block
	ctrl->next = start + 1;

	controller->DataOut();
}

void Disk::Write6(Controller *controller)
{
	Write(controller, RW6);
}

void Disk::Write10(Controller *controller)
{
	Write(controller, RW10);
}

void Disk::Write16(Controller *controller)
{
	Write(controller, RW16);
}

void Disk::Verify(Controller *controller, access_mode mode)
{
	uint64_t start;
	if (!GetStartAndCount(controller, start, ctrl->blocks, mode)) {
		return;
	}

	// if BytChk=0
	if ((ctrl->cmd[1] & 0x02) == 0) {
		Seek(controller);
		return;
	}

	// Test loading
	ctrl->length = Read(ctrl->cmd, ctrl->buffer, start);

	// Set next block
	ctrl->next = start + 1;

	controller->DataOut();
}

void Disk::Verify10(Controller *controller)
{
	Verify(controller, RW10);
}

void Disk::Verify16(Controller *controller)
{
	Verify(controller, RW16);
}

void Disk::StartStopUnit(Controller *controller)
{
	if (!StartStop(ctrl->cmd)) {
		throw scsi_dispatch_error_exception();
	}

	controller->Status();
}

void Disk::SendDiagnostic(Controller *controller)
{
	if (!SendDiag(ctrl->cmd)) {
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	controller->Status();
}

void Disk::PreventAllowMediumRemoval(Controller *controller)
{
	CheckReady();

	bool lock = ctrl->cmd[4] & 0x01;

	LOGTRACE("%s", lock ? "Locking medium" : "Unlocking medium");

	SetLocked(lock);

	controller->Status();
}

void Disk::SynchronizeCache10(Controller *controller)
{
	FlushCache();

	controller->Status();
}

void Disk::SynchronizeCache16(Controller *controller)
{
	return SynchronizeCache10(controller);
}

void Disk::ReadDefectData10(Controller *controller)
{
	int allocation_length = (ctrl->cmd[7] << 8) | ctrl->cmd[8];
	if (allocation_length > 4) {
		allocation_length = 4;
	}

	// The defect list is empty
	memset(ctrl->buffer, 0, allocation_length);
	ctrl->length = allocation_length;

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

	size += super::AddModePages(cdb, &buf[size], length - size);
	if (size > 255) {
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

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

	size += super::AddModePages(cdb, &buf[size], length - size);
	if (size > 65535) {
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

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

	// No changeable area
	if (changeable) {
		pages[3] = buf;

		return;
	}

	if (IsReady()) {
		// Set the number of tracks in one zone to 8 (TODO)
		buf[0x03] = 0x08;

		// Set sector/track to 25 (TODO)
		buf[0x0a] = 0x00;
		buf[0x0b] = 0x19;

		// Set the number of bytes in the physical sector
		int size = 1 << disk.size;
		buf[0x0c] = (BYTE)(size >> 8);
		buf[0x0d] = (BYTE)size;

		// Interleave 1
		buf[0x0e] = 0x00;
		buf[0x0f] = 0x01;

		// Track skew factor 11
		buf[0x10] = 0x00;
		buf[0x11] = 0x0b;

		// Cylinder skew factor 20
		buf[0x12] = 0x00;
		buf[0x13] = 0x14;
	}

	buf[20] = IsRemovable() ? 0x20 : 0x00;

	// Hard-sectored
	buf[20] |= 0x40;

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
		buf[0x02] = (BYTE)(cylinder >> 16);
		buf[0x03] = (BYTE)(cylinder >> 8);
		buf[0x04] = (BYTE)cylinder;

		// Fix the head at 8
		buf[0x05] = 0x8;

		// Medium rotation rate 7200
		buf[0x14] = 0x1c;
		buf[0x15] = 0x20;
	}

	pages[4] = buf;
}

void Disk::AddCachePage(map<int, vector<BYTE>>& pages, bool changeable) const
{
	vector<BYTE> buf(12);

	// No changeable area
	if (changeable) {
		pages[8] = buf;

		return;
	}

	// Only read cache is valid

	// Disable pre-fetch transfer length
	buf[0x04] = 0xff;
	buf[0x05] = 0xff;

	// Maximum pre-fetch
	buf[0x08] = 0xff;
	buf[0x09] = 0xff;

	// Maximum pre-fetch ceiling
	buf[0x0a] = 0xff;
	buf[0x0b] = 0xff;

	pages[8] = buf;
}

void Disk::AddVendorPage(map<int, vector<BYTE>>&, int, bool) const
{
	// Nothing to add by default
}

void Disk::Format(const DWORD *cdb)
{
	CheckReady();

	// FMTDATA=1 is not supported (but OK if there is no DEFECT LIST)
	if ((cdb[1] & 0x10) != 0 && cdb[4] != 0) {
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}
}

// TODO Read more than one block in a single call. Currently blocked by the the track-oriented cache
int Disk::Read(const DWORD *cdb, BYTE *buf, uint64_t block)
{
	LOGTRACE("%s", __PRETTY_FUNCTION__);

	CheckReady();

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		 throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// leave it to the cache
	if (!disk.dcache->ReadSector(buf, block)) {
		throw scsi_dispatch_error_exception(sense_key::MEDIUM_ERROR, asc::READ_FAULT);
	}

	//  Success
	return 1 << disk.size;
}

int Disk::WriteCheck(DWORD block)
{
	CheckReady();

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Error if write protected
	if (IsProtected()) {
		throw scsi_dispatch_error_exception(sense_key::DATA_PROTECT, asc::WRITE_PROTECTED);
	}

	//  Success
	return 1 << disk.size;
}

// TODO Write more than one block in a single call. Currently blocked by the track-oriented cache
bool Disk::Write(Controller *controller, const DWORD *cdb, const BYTE *buf, DWORD block)
{
	LOGTRACE("%s", __PRETTY_FUNCTION__);

	// Error if not ready
	if (!IsReady()) {
		controller->Error(sense_key::NOT_READY);
		return false;
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		controller->Error(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
		return false;
	}

	// Error if write protected
	if (IsProtected()) {
		controller->Error(sense_key::DATA_PROTECT, asc::WRITE_PROTECTED);
		return false;
	}

	// Leave it to the cache
	if (!disk.dcache->WriteSector(buf, block)) {
		controller->Error(sense_key::MEDIUM_ERROR, asc::WRITE_FAULT);
		return false;
	}

	return true;
}

void Disk::Seek(Controller *controller)
{
	CheckReady();

	controller->Status();
}

void Disk::Seek6(Controller *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, SEEK6)) {
		Seek(controller);
	}
}

void Disk::Seek10(Controller *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, SEEK10)) {
		Seek(controller);
	}
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
				throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::LOAD_OR_EJECT_FAILED);
			}

			// Eject
			return Eject(false);
		}
	}

	return true;
}

bool Disk::SendDiag(const DWORD *cdb) const
{
	// Do not support PF bit
	if (cdb[1] & 0x10) {
		return false;
	}

	// Do not support parameter list
	if ((cdb[3] != 0) || (cdb[4] != 0)) {
		return false;
	}

	return true;
}

void Disk::ReadCapacity10(Controller *controller)
{
	CheckReady();

	if (disk.blocks <= 0) {
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::MEDIUM_NOT_PRESENT);
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

void Disk::ReadCapacity16(Controller *controller)
{
	CheckReady();

	if (disk.blocks <= 0) {
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::MEDIUM_NOT_PRESENT);
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

void Disk::ReadCapacity16_ReadLong16(Controller *controller)
{
	// The service action determines the actual command
	switch (ctrl->cmd[1] & 0x1f) {
	case 0x10:
		ReadCapacity16(controller);
		break;

	case 0x11:
		ReadWriteLong16(controller);
		break;

	default:
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
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
void Disk::Reserve(Controller *controller)
{
	controller->Status();
}

void Disk::Release(Controller *controller)
{
	controller->Status();
}

//---------------------------------------------------------------------------
//
//	Check/Get start sector and sector count for a READ/WRITE or READ/WRITE LONG operation
//
//---------------------------------------------------------------------------

void Disk::ValidateBlockAddress(Controller *controller, access_mode mode)
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
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
	}
}

bool Disk::GetStartAndCount(Controller *controller, uint64_t& start, uint32_t& count, access_mode mode)
{
	if (mode == RW6 || mode == SEEK6) {
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
		else if (mode != SEEK6 && mode != SEEK10) {
			count = ctrl->cmd[7];
			count <<= 8;
			count |= ctrl->cmd[8];
		}
		else {
			count = 0;
		}
	}

	LOGTRACE("%s READ/WRITE/VERIFY/SEEK command record=$%08X blocks=%d", __PRETTY_FUNCTION__, (uint32_t)start, count);

	// Check capacity
	uint64_t capacity = GetBlockCount();
	if (start > capacity || start + count > capacity) {
		LOGTRACE("%s", ("Capacity of " + to_string(capacity) + " blocks exceeded: Trying to access block "
				+ to_string(start) + ", block count " + to_string(count)).c_str());
		throw scsi_dispatch_error_exception(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
	}

	// Do not process 0 blocks
	if (!count && (mode != SEEK6 && mode != SEEK10)) {
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

void Disk::SetSectorSizeInBytes(uint32_t size)
{
	unordered_set<uint32_t> sector_sizes = DeviceFactory::instance().GetSectorSizes(GetType());
	if (!sector_sizes.empty() && sector_sizes.find(size) == sector_sizes.end()) {
		throw io_exception("Invalid block size of " + to_string(size) + " bytes");
	}

	switch (size) {
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

void Disk::SetSectorSizes(const unordered_set<uint32_t>& sector_sizes)
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

	unordered_set<uint32_t> sector_sizes = device_factory.GetSectorSizes(GetType());
	if (sector_sizes.find(configured_sector_size) == sector_sizes.end()) {
		return false;
	}

	this->configured_sector_size = configured_sector_size;

	return true;
}

void Disk::SetGeometries(const unordered_map<uint64_t, Geometry>& geometries)
{
	this->geometries = geometries;
}

bool Disk::SetGeometryForCapacity(uint64_t capacity) {
	const auto& geometry = geometries.find(capacity);
	if (geometry != geometries.end()) {
		SetSectorSizeInBytes(geometry->second.first);
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
