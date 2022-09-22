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
#include "rascsi_exceptions.h"
#include "disk.h"
#include "mode_page_device.h"
#include "scsi_command_util.h"

using namespace scsi_defs;
using namespace scsi_command_util;

Disk::Disk(const string& id) : ModePageDevice(id), ScsiBlockCommands()
{
	dispatcher.Add(scsi_command::eCmdRezero, "Rezero", &Disk::Rezero);
	dispatcher.Add(scsi_command::eCmdFormat, "FormatUnit", &Disk::FormatUnit);
	dispatcher.Add(scsi_command::eCmdReassign, "ReassignBlocks", &Disk::ReassignBlocks);
	dispatcher.Add(scsi_command::eCmdRead6, "Read6", &Disk::Read6);
	dispatcher.Add(scsi_command::eCmdWrite6, "Write6", &Disk::Write6);
	dispatcher.Add(scsi_command::eCmdSeek6, "Seek6", &Disk::Seek6);
	dispatcher.Add(scsi_command::eCmdReserve6, "Reserve6", &Disk::Reserve);
	dispatcher.Add(scsi_command::eCmdRelease6, "Release6", &Disk::Release);
	dispatcher.Add(scsi_command::eCmdStartStop, "StartStopUnit", &Disk::StartStopUnit);
	dispatcher.Add(scsi_command::eCmdSendDiag, "SendDiagnostic", &Disk::SendDiagnostic);
	dispatcher.Add(scsi_command::eCmdRemoval, "PreventAllowMediumRemoval", &Disk::PreventAllowMediumRemoval);
	dispatcher.Add(scsi_command::eCmdReadCapacity10, "ReadCapacity10", &Disk::ReadCapacity10);
	dispatcher.Add(scsi_command::eCmdRead10, "Read10", &Disk::Read10);
	dispatcher.Add(scsi_command::eCmdWrite10, "Write10", &Disk::Write10);
	dispatcher.Add(scsi_command::eCmdReadLong10, "ReadLong10", &Disk::ReadWriteLong10);
	dispatcher.Add(scsi_command::eCmdWriteLong10, "WriteLong10", &Disk::ReadWriteLong10);
	dispatcher.Add(scsi_command::eCmdWriteLong16, "WriteLong16", &Disk::ReadWriteLong16);
	dispatcher.Add(scsi_command::eCmdSeek10, "Seek10", &Disk::Seek10);
	dispatcher.Add(scsi_command::eCmdVerify10, "Verify10", &Disk::Verify10);
	dispatcher.Add(scsi_command::eCmdSynchronizeCache10, "SynchronizeCache10", &Disk::SynchronizeCache);
	dispatcher.Add(scsi_command::eCmdSynchronizeCache16, "SynchronizeCache16", &Disk::SynchronizeCache);
	dispatcher.Add(scsi_command::eCmdReadDefectData10, "ReadDefectData10", &Disk::ReadDefectData10);
	dispatcher.Add(scsi_command::eCmdReserve10, "Reserve10", &Disk::Reserve);
	dispatcher.Add(scsi_command::eCmdRelease10, "Release10", &Disk::Release);
	dispatcher.Add(scsi_command::eCmdRead16, "Read16", &Disk::Read16);
	dispatcher.Add(scsi_command::eCmdWrite16, "Write16", &Disk::Write16);
	dispatcher.Add(scsi_command::eCmdVerify16, "Verify16", &Disk::Verify16);
	dispatcher.Add(scsi_command::eCmdReadCapacity16_ReadLong16, "ReadCapacity16/ReadLong16", &Disk::ReadCapacity16_ReadLong16);
}

Disk::~Disk()
{
	// Save disk cache, only if ready
	if (IsReady() && disk.dcache) {
		disk.dcache->Save();
	}

	delete disk.dcache;
}

bool Disk::Dispatch(scsi_command cmd)
{
	// Media changes must be reported on the next access, i.e. not only for TEST UNIT READY
	if (disk.is_medium_changed) {
		assert(IsRemovable());

		disk.is_medium_changed = false;

		throw scsi_error_exception(sense_key::UNIT_ATTENTION, asc::NOT_READY_TO_READY_CHANGE);
	}

	// The superclass handles the less specific commands
	return dispatcher.Dispatch(this, cmd) ? true : super::Dispatch(cmd);
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
	disk.dcache = new DiskCache(path, disk.size, (uint32_t)disk.blocks, disk.image_offset);

	// Can read/write open
	if (Fileio fio; fio.Open(path, Fileio::OpenMode::ReadWrite)) {
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

void Disk::Rezero()
{
	Seek();
}

void Disk::FormatUnit()
{
	CheckReady();

	// FMTDATA=1 is not supported (but OK if there is no DEFECT LIST)
	if ((ctrl->cmd[1] & 0x10) != 0 && ctrl->cmd[4] != 0) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	EnterStatusPhase();
}

void Disk::ReassignBlocks()
{
	Seek();
}

void Disk::Read(access_mode mode)
{
	if (uint64_t start; CheckAndGetStartAndCount(start, ctrl->blocks, mode)) {
		ctrl->length = Read(ctrl->cmd, ctrl->buffer, start);
		LOGTRACE("%s ctrl.length is %d", __PRETTY_FUNCTION__, ctrl->length)

		// Set next block
		ctrl->next = start + 1;

		EnterDataInPhase();
	}
	else {
		EnterStatusPhase();
	}
}

void Disk::Read6()
{
	Read(RW6);
}

void Disk::Read10()
{
	Read(RW10);
}

void Disk::Read16()
{
	Read(RW16);
}

void Disk::ReadWriteLong10()
{
	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (ctrl->cmd[7] || ctrl->cmd[8]) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	ValidateBlockAddress(RW10);

	EnterStatusPhase();
}

void Disk::ReadWriteLong16()
{
	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (ctrl->cmd[12] || ctrl->cmd[13]) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	ValidateBlockAddress(RW16);

	EnterStatusPhase();
}

void Disk::Write(access_mode mode)
{
	if (uint64_t start; CheckAndGetStartAndCount(start, ctrl->blocks, mode)) {
		ctrl->length = WriteCheck(start);

		// Set next block
		ctrl->next = start + 1;

		EnterDataOutPhase();
	}
	else {
		EnterStatusPhase();
	}
}

void Disk::Write6()
{
	Write(RW6);
}

void Disk::Write10()
{
	Write(RW10);
}

void Disk::Write16()
{
	Write(RW16);
}

void Disk::Verify(access_mode mode)
{
	if (uint64_t start; CheckAndGetStartAndCount(start, ctrl->blocks, mode)) {
		// if BytChk=0
		if ((ctrl->cmd[1] & 0x02) == 0) {
			Seek();
			return;
		}

		// Test reading
		ctrl->length = Read(ctrl->cmd, ctrl->buffer, start);

		// Set next block
		ctrl->next = start + 1;

		EnterDataOutPhase();
	}
	else {
		EnterStatusPhase();
	}
}

void Disk::Verify10()
{
	Verify(RW10);
}

void Disk::Verify16()
{
	Verify(RW16);
}

void Disk::StartStopUnit()
{
	bool start = ctrl->cmd[4] & 0x01;
	bool load = ctrl->cmd[4] & 0x02;

	if (load) {
		LOGTRACE("%s", start ? "Loading medium" : "Ejecting medium")
	}
	else {
		LOGTRACE("%s", start ? "Starting unit" : "Stopping unit")

		SetStopped(!start);
	}

	if (!start) {
		FlushCache();

		// Look at the eject bit and eject if necessary
		if (load) {
			if (IsLocked()) {
				// Cannot be ejected because it is locked
				throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::LOAD_OR_EJECT_FAILED);
			}

			// Eject
			if (!Eject(false)) {
				throw scsi_error_exception();
			}
		}
	}

	EnterStatusPhase();
}

void Disk::SendDiagnostic()
{
	// Do not support PF bit
	if (ctrl->cmd[1] & 0x10) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Do not support parameter list
	if ((ctrl->cmd[3] != 0) || (ctrl->cmd[4] != 0)) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	EnterStatusPhase();
}

void Disk::PreventAllowMediumRemoval()
{
	CheckReady();

	bool lock = ctrl->cmd[4] & 0x01;

	LOGTRACE("%s", lock ? "Locking medium" : "Unlocking medium")

	SetLocked(lock);

	EnterStatusPhase();
}

void Disk::SynchronizeCache()
{
	FlushCache();

	EnterStatusPhase();
}

void Disk::ReadDefectData10()
{
	int allocation_length = GetInt16(ctrl->cmd, 7);
	if (allocation_length > 4) {
		allocation_length = 4;
	}

	// The defect list is empty
	memset(ctrl->buffer, 0, allocation_length);
	ctrl->length = allocation_length;

	EnterDataInPhase();
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
	bool status = super::Eject(force);
	if (status) {
		FlushCache();
		delete disk.dcache;
		disk.dcache = nullptr;

		// The image file for this drive is not in use anymore
		// TODO This cast and the FileSupport class can be removed as soon as only disk-like devices inherit from Disk
		if (auto file_support = dynamic_cast<FileSupport *>(this); file_support) {
			file_support->UnreserveFile();
		}
	}

	return status;
}

int Disk::ModeSense6(const vector<int>& cdb, BYTE *buf, int max_length) const
{
	// Get length, clear buffer
	auto length = cdb[4];
	if (length > max_length) {
		length = max_length;
	}
	memset(buf, 0, length);

	// DEVICE SPECIFIC PARAMETER
	if (IsProtected()) {
		buf[2] = 0x80;
	}

	// Basic information
	int size = 4;

	// Add block descriptor if DBD is 0
	if ((cdb[1] & 0x08) == 0) {
		// Mode parameter header, block descriptor length
		buf[3] = 0x08;

		// Only if ready
		if (IsReady()) {
			// Short LBA mode parameter block descriptor (number of blocks and block length)
			SetInt32(&buf[4], (uint32_t)GetBlockCount());
			SetInt32(&buf[8], GetSectorSizeInBytes());
		}

		size = 12;
	}

	size += super::AddModePages(cdb, &buf[size], length - size);
	if (size > 255) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Do not return more than ALLOCATION LENGTH bytes
	if (size > length) {
		size = length;
	}

	// Final setting of mode data length
	buf[0] = (BYTE)size;

	return size;
}

int Disk::ModeSense10(const vector<int>& cdb, BYTE *buf, int max_length) const
{
	// Get length, clear buffer
	int length = GetInt16(cdb, 7);
	if (length > max_length) {
		length = max_length;
	}
	memset(buf, 0, length);

	// DEVICE SPECIFIC PARAMETER
	if (IsProtected()) {
		buf[3] = 0x80;
	}

	// Basic Information
	int size = 8;

	// Add block descriptor if DBD is 0, only if ready
	if ((cdb[1] & 0x08) == 0 && IsReady()) {
		uint64_t disk_blocks = GetBlockCount();
		uint32_t disk_size = GetSectorSizeInBytes();

		// Check LLBAA for short or long block descriptor
		if ((cdb[1] & 0x10) == 0 || disk_blocks <= 0xFFFFFFFF) {
			// Mode parameter header, block descriptor length
			buf[7] = 0x08;

			// Short LBA mode parameter block descriptor (number of blocks and block length)
			SetInt32(&buf[8], (uint32_t)disk_blocks);
			SetInt32(&buf[12], disk_size);

			size = 16;
		}
		else {
			// Mode parameter header, LONGLBA
			buf[4] = 0x01;

			// Mode parameter header, block descriptor length
			buf[7] = 0x10;

			// Long LBA mode parameter block descriptor (number of blocks and block length)
			SetInt64(&buf[8], disk_blocks);
			SetInt32(&buf[20], disk_size);

			size = 24;
		}
	}

	size += super::AddModePages(cdb, &buf[size], length - size);
	if (size > 65535) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Do not return more than ALLOCATION LENGTH bytes
	if (size > length) {
		size = length;
	}

	// Final setting of mode data length
	SetInt16(buf, size);

	return size;
}

void Disk::AddModePages(map<int, vector<byte>>& pages, int page, bool changeable) const
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

void Disk::AddErrorPage(map<int, vector<byte>>& pages, bool) const
{
	vector<byte> buf(12);

	// Retry count is 0, limit time uses internal default value

	pages[1] = buf;
}

void Disk::AddFormatPage(map<int, vector<byte>>& pages, bool changeable) const
{
	vector<byte> buf(24);

	// No changeable area
	if (changeable) {
		pages[3] = buf;

		return;
	}

	if (IsReady()) {
		// Set the number of tracks in one zone to 8
		buf[0x03] = (byte)0x08;

		// Set sector/track to 25
		SetInt16(buf, 0x0a, 25);

		// Set the number of bytes in the physical sector
		SetInt16(buf, 0x0c, 1 << disk.size);

		// Interleave 1
		SetInt16(buf, 0x0e, 1);

		// Track skew factor 11
		SetInt16(buf, 0x10, 11);

		// Cylinder skew factor 20
		SetInt16(buf, 0x12, 20);
	}

	buf[20] = IsRemovable() ? (byte)0x20 : (byte)0x00;

	// Hard-sectored
	buf[20] |= (byte)0x40;

	pages[3] = buf;
}

void Disk::AddDrivePage(map<int, vector<byte>>& pages, bool changeable) const
{
	vector<byte> buf(24);

	// No changeable area
	if (changeable) {
		pages[4] = buf;

		return;
	}

	if (IsReady()) {
		// Set the number of cylinders (total number of blocks
        // divided by 25 sectors/track and 8 heads)
		uint64_t cylinders = disk.blocks;
		cylinders >>= 3;
		cylinders /= 25;
		SetInt32(buf, 0x01, (uint32_t)cylinders);

		// Fix the head at 8
		buf[0x05] = (byte)0x8;

		// Medium rotation rate 7200
		SetInt16(buf, 0x14, 7200);
	}

	pages[4] = buf;
}

void Disk::AddCachePage(map<int, vector<byte>>& pages, bool changeable) const
{
	vector<byte> buf(12);

	// No changeable area
	if (changeable) {
		pages[8] = buf;

		return;
	}

	// Only read cache is valid

	// Disable pre-fetch transfer length
	SetInt16(buf, 0x04, -1);

	// Maximum pre-fetch
	SetInt16(buf, 0x08, -1);

	// Maximum pre-fetch ceiling
	SetInt16(buf, 0x0a, -1);

	pages[8] = buf;
}

void Disk::AddVendorPage(map<int, vector<byte>>&, int, bool) const
{
	// Nothing to add by default
}

// TODO Read more than one block in a single call. Currently blocked by the the track-oriented cache
int Disk::Read(const vector<int>&, BYTE *buf, uint64_t block)
{
	LOGTRACE("%s", __PRETTY_FUNCTION__)

	CheckReady();

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		 throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// leave it to the cache
	if (!disk.dcache->ReadSector(buf, (uint32_t)block)) {
		throw scsi_error_exception(sense_key::MEDIUM_ERROR, asc::READ_FAULT);
	}

	//  Success
	return 1 << disk.size;
}

int Disk::WriteCheck(uint64_t block)
{
	CheckReady();

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Error if write protected
	if (IsProtected()) {
		throw scsi_error_exception(sense_key::DATA_PROTECT, asc::WRITE_PROTECTED);
	}

	//  Success
	return 1 << disk.size;
}

// TODO Write more than one block in a single call. Currently blocked by the track-oriented cache
void Disk::Write(const vector<int>&, const BYTE *buf, uint64_t block)
{
	LOGTRACE("%s", __PRETTY_FUNCTION__)

	// Error if not ready
	if (!IsReady()) {
		throw scsi_error_exception(sense_key::NOT_READY);
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
	}

	// Error if write protected
	if (IsProtected()) {
		throw scsi_error_exception(sense_key::DATA_PROTECT, asc::WRITE_PROTECTED);
	}

	// Leave it to the cache
	if (!disk.dcache->WriteSector(buf, (uint32_t)block)) {
		throw scsi_error_exception(sense_key::MEDIUM_ERROR, asc::WRITE_FAULT);
	}
}

void Disk::Seek()
{
	CheckReady();

	EnterStatusPhase();
}

void Disk::Seek6()
{
	if (uint64_t start; CheckAndGetStartAndCount(start, ctrl->blocks, SEEK6)) {
		CheckReady();
	}

	EnterStatusPhase();
}

void Disk::Seek10()
{
	if (uint64_t start; CheckAndGetStartAndCount(start, ctrl->blocks, SEEK10)) {
		CheckReady();
	}

	EnterStatusPhase();
}

void Disk::ReadCapacity10()
{
	CheckReady();

	if (disk.blocks == 0) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::MEDIUM_NOT_PRESENT);
	}

	BYTE *buf = ctrl->buffer;

	// Create end of logical block address (disk.blocks-1)
	uint64_t blocks = disk.blocks - 1;

	// If the capacity exceeds 32 bit, -1 must be returned and the client has to use READ CAPACITY(16)
	if (blocks > 4294967295) {
		blocks = -1;
	}
	SetInt32(&buf[0], (uint32_t)blocks);

	// Create block length (1 << disk.size)
	SetInt32(&buf[4], 1 << disk.size);

	// the size
	ctrl->length = 8;

	EnterDataInPhase();
}

void Disk::ReadCapacity16()
{
	CheckReady();

	if (disk.blocks == 0) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::MEDIUM_NOT_PRESENT);
	}

	BYTE *buf = ctrl->buffer;

	// Create end of logical block address (disk.blocks-1)
	SetInt64(&buf[0], disk.blocks - 1);

	// Create block length (1 << disk.size)
	SetInt32(&buf[8], 1 << disk.size);

	buf[12] = 0;

	// Logical blocks per physical block: not reported (1 or more)
	buf[13] = 0;

	// the size
	ctrl->length = 14;

	EnterDataInPhase();
}

void Disk::ReadCapacity16_ReadLong16()
{
	// The service action determines the actual command
	switch (ctrl->cmd[1] & 0x1f) {
	case 0x10:
		ReadCapacity16();
		break;

	case 0x11:
		ReadWriteLong16();
		break;

	default:
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
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
void Disk::Reserve()
{
	EnterStatusPhase();
}

void Disk::Release()
{
	EnterStatusPhase();
}

//---------------------------------------------------------------------------
//
//	Check/Get start sector and sector count for a READ/WRITE or READ/WRITE LONG operation
//
//---------------------------------------------------------------------------

void Disk::ValidateBlockAddress(access_mode mode) const
{
	uint64_t block = mode == RW16 ? GetInt64(ctrl->cmd, 2) : GetInt32(ctrl->cmd, 2);

	uint64_t capacity = GetBlockCount();

	if (block > capacity) {
		LOGTRACE("%s", ("Capacity of " + to_string(capacity) + " block(s) exceeded: Trying to access block "
				+ to_string(block)).c_str())
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
	}
}

bool Disk::CheckAndGetStartAndCount(uint64_t& start, uint32_t& count, access_mode mode) const
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
		start = mode == RW16 ? GetInt64(ctrl->cmd, 2) : GetInt32(ctrl->cmd, 2);

		if (mode == RW16) {
			count = GetInt32(ctrl->cmd, 10);
		}
		else if (mode != SEEK6 && mode != SEEK10) {
			count = GetInt16(ctrl->cmd, 7);
		}
		else {
			count = 0;
		}
	}

	LOGTRACE("%s READ/WRITE/VERIFY/SEEK command record=$%08X blocks=%d", __PRETTY_FUNCTION__, (uint32_t)start, count)

	// Check capacity
	if (uint64_t capacity = GetBlockCount(); start > capacity || start + count > capacity) {
		LOGTRACE("%s", ("Capacity of " + to_string(capacity) + " block(s) exceeded: Trying to access block "
				+ to_string(start) + ", block count " + to_string(count)).c_str())
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
	}

	// Do not process 0 blocks
	if (!count && (mode != SEEK6 && mode != SEEK10)) {
		return false;
	}

	return true;
}

uint32_t Disk::GetSectorSizeInBytes() const
{
	return disk.size ? 1 << disk.size : 0;
}

void Disk::SetSectorSizeInBytes(uint32_t size_in_bytes)
{
	if (unordered_set<uint32_t> sizes = DeviceFactory::instance().GetSectorSizes(GetType());
		!sizes.empty() && sizes.find(size_in_bytes) == sizes.end()) {
		throw io_exception("Invalid block size of " + to_string(size_in_bytes) + " bytes");
	}

	switch (size_in_bytes) {
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

void Disk::SetSectorSizeShiftCount(uint32_t shift_count)
{
	disk.size = shift_count;
}

bool Disk::IsSectorSizeConfigurable() const
{
	return !sector_sizes.empty();
}

void Disk::SetSectorSizes(const unordered_set<uint32_t>& sizes)
{
	sector_sizes = sizes;
}

uint32_t Disk::GetConfiguredSectorSize() const
{
	return configured_sector_size;
}

bool Disk::SetConfiguredSectorSize(const DeviceFactory& device_factory, uint32_t size)
{
	if (unordered_set<uint32_t> sizes = device_factory.GetSectorSizes(GetType());
		sizes.find(size) == sizes.end()) {
		return false;
	}

	configured_sector_size = size;

	return true;
}

uint64_t Disk::GetBlockCount() const
{
	return disk.blocks;
}

void Disk::SetBlockCount(uint64_t blocks)
{
	disk.blocks = blocks;
}
