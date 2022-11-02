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

#include "rascsi_exceptions.h"
#include "dispatcher.h"
#include "scsi_command_util.h"
#include "disk.h"

using namespace scsi_defs;
using namespace scsi_command_util;

const unordered_map<uint32_t, uint32_t> Disk::shift_counts = { { 512, 9 }, { 1024, 10 }, { 2048, 11 }, { 4096, 12 } };

Disk::Disk(PbDeviceType type, int lun) : StorageDevice(type, lun)
{
	// REZERO implementation is identical with Seek
	dispatcher.Add(scsi_command::eCmdRezero, "Rezero", &Disk::Seek);
	dispatcher.Add(scsi_command::eCmdFormat, "FormatUnit", &Disk::FormatUnit);
	// REASSIGN BLOCKS implementation is identical with Seek
	dispatcher.Add(scsi_command::eCmdReassign, "ReassignBlocks", &Disk::Seek);
	dispatcher.Add(scsi_command::eCmdRead6, "Read6", &Disk::Read6);
	dispatcher.Add(scsi_command::eCmdWrite6, "Write6", &Disk::Write6);
	dispatcher.Add(scsi_command::eCmdSeek6, "Seek6", &Disk::Seek6);
	dispatcher.Add(scsi_command::eCmdStartStop, "StartStopUnit", &Disk::StartStopUnit);
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
	dispatcher.Add(scsi_command::eCmdRead16, "Read16", &Disk::Read16);
	dispatcher.Add(scsi_command::eCmdWrite16, "Write16", &Disk::Write16);
	dispatcher.Add(scsi_command::eCmdVerify16, "Verify16", &Disk::Verify16);
	dispatcher.Add(scsi_command::eCmdReadCapacity16_ReadLong16, "ReadCapacity16/ReadLong16", &Disk::ReadCapacity16_ReadLong16);
}

Disk::~Disk()
{
	// Save disk cache, only if ready
	if (IsReady() && cache != nullptr) {
		cache->Save();
	}
}

bool Disk::Dispatch(scsi_command cmd)
{
	// Media changes must be reported on the next access, i.e. not only for TEST UNIT READY
	if (IsMediumChanged()) {
		assert(IsRemovable());

		SetMediumChanged(false);

		throw scsi_exception(sense_key::UNIT_ATTENTION, asc::NOT_READY_TO_READY_CHANGE);
	}

	// The superclass handles the less specific commands
	return dispatcher.Dispatch(this, cmd) ? true : super::Dispatch(cmd);
}

void Disk::SetUpCache(off_t image_offset, bool raw)
{
	cache = make_unique<DiskCache>(GetFilename(), size_shift_count, static_cast<uint32_t>(GetBlockCount()), image_offset);
	cache->SetRawMode(raw);
}

void Disk::ResizeCache(const string& path, bool raw)
{
	cache.reset(new DiskCache(path, size_shift_count, static_cast<uint32_t>(GetBlockCount())));
	cache->SetRawMode(raw);
}

void Disk::FlushCache()
{
	if (cache != nullptr) {
		cache->Save();
	}
}

void Disk::FormatUnit()
{
	CheckReady();

	// FMTDATA=1 is not supported (but OK if there is no DEFECT LIST)
	if ((controller->GetCmd(1) & 0x10) != 0 && controller->GetCmd(4) != 0) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	EnterStatusPhase();
}

void Disk::Read(access_mode mode)
{
	uint64_t start;
	uint32_t blocks;
	if (CheckAndGetStartAndCount(start, blocks, mode)) {
		controller->SetBlocks(blocks);
		controller->SetLength(Read(controller->GetCmd(), controller->GetBuffer(), start));
		LOGTRACE("%s ctrl.length is %d", __PRETTY_FUNCTION__, controller->GetLength())

		// Set next block
		controller->SetNext(start + 1);

		EnterDataInPhase();
	}
	else {
		EnterStatusPhase();
	}
}

void Disk::ReadWriteLong10()
{
	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (GetInt16(controller->GetCmd(), 7) != 0) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	ValidateBlockAddress(RW10);

	EnterStatusPhase();
}

void Disk::ReadWriteLong16()
{
	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (GetInt16(controller->GetCmd(), 12) != 0) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	ValidateBlockAddress(RW16);

	EnterStatusPhase();
}

void Disk::Write(access_mode mode)
{
	uint64_t start;
	uint32_t blocks;
	if (CheckAndGetStartAndCount(start, blocks, mode)) {
		controller->SetBlocks(blocks);
		controller->SetLength(WriteCheck(start));

		// Set next block
		controller->SetNext(start + 1);

		EnterDataOutPhase();
	}
	else {
		EnterStatusPhase();
	}
}

void Disk::Verify(access_mode mode)
{
	uint64_t start;
	uint32_t blocks;
	if (CheckAndGetStartAndCount(start, blocks, mode)) {
		// if BytChk=0
		if ((controller->GetCmd(1) & 0x02) == 0) {
			Seek();
			return;
		}

		// Test reading
		controller->SetBlocks(blocks);
		controller->SetLength(Read(controller->GetCmd(), controller->GetBuffer(), start));

		// Set next block
		controller->SetNext(start + 1);

		EnterDataOutPhase();
	}
	else {
		EnterStatusPhase();
	}
}

void Disk::StartStopUnit()
{
	const bool start = controller->GetCmd(4) & 0x01;
	const bool load = controller->GetCmd(4) & 0x02;

	if (load) {
		LOGTRACE(start ? "Loading medium" : "Ejecting medium")
	}
	else {
		LOGTRACE(start ? "Starting unit" : "Stopping unit")

		SetStopped(!start);
	}

	if (!start) {
		// Look at the eject bit and eject if necessary
		if (load) {
			if (IsLocked()) {
				// Cannot be ejected because it is locked
				throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::LOAD_OR_EJECT_FAILED);
			}

			// Eject
			if (!Eject(false)) {
				throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::LOAD_OR_EJECT_FAILED);
			}
		}
		else {
			FlushCache();
		}
	}

	EnterStatusPhase();
}

void Disk::PreventAllowMediumRemoval()
{
	CheckReady();

	const bool lock = controller->GetCmd(4) & 0x01;

	LOGTRACE(lock ? "Locking medium" : "Unlocking medium")

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
	const size_t allocation_length = min(static_cast<size_t>(GetInt16(controller->GetCmd(), 7)), static_cast<size_t>(4));

	// The defect list is empty
	fill_n(controller->GetBuffer().begin(), allocation_length, 0);
	controller->SetLength(static_cast<uint32_t>(allocation_length));

	EnterDataInPhase();
}

bool Disk::Eject(bool force)
{
	const bool status = super::Eject(force);
	if (status) {
		FlushCache();
		cache.reset();

		// The image file for this drive is not in use anymore
		UnreserveFile();
	}

	return status;
}

int Disk::ModeSense6(const vector<int>& cdb, vector<uint8_t>& buf) const
{
	// Get length, clear buffer
	const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(cdb[4])));
	fill_n(buf.begin(), length, 0);

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
			SetInt32(buf, 4, static_cast<uint32_t>(GetBlockCount()));
			SetInt32(buf, 8, GetSectorSizeInBytes());
		}

		size = 12;
	}

	size = AddModePages(cdb, buf, size, length, 255);

	buf[0] = (uint8_t)size;

	return size;
}

int Disk::ModeSense10(const vector<int>& cdb, vector<uint8_t>& buf) const
{
	// Get length, clear buffer
	const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(GetInt16(cdb, 7))));
	fill_n(buf.begin(), length, 0);

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
			SetInt32(buf, 8, static_cast<uint32_t>(disk_blocks));
			SetInt32(buf, 12, disk_size);

			size = 16;
		}
		else {
			// Mode parameter header, LONGLBA
			buf[4] = 0x01;

			// Mode parameter header, block descriptor length
			buf[7] = 0x10;

			// Long LBA mode parameter block descriptor (number of blocks and block length)
			SetInt64(buf, 8, disk_blocks);
			SetInt32(buf, 20, disk_size);

			size = 24;
		}
	}

	size = AddModePages(cdb, buf, size, length, 65535);

	SetInt16(buf, 0, size);

	return size;
}

void Disk::SetUpModePages(map<int, vector<byte>>& pages, int page, bool changeable) const
{
	// Page code 1 (read-write error recovery)
	if (page == 0x01 || page == 0x3f) {
		AddErrorPage(pages, changeable);
	}

	// Page code 3 (format device)
	if (page == 0x03 || page == 0x3f) {
		AddFormatPage(pages, changeable);
	}

	// Page code 4 (rigid drive page)
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
		SetInt16(buf, 0x0c, 1 << size_shift_count);

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
		uint64_t cylinders = GetBlockCount();
		cylinders >>= 3;
		cylinders /= 25;
		SetInt32(buf, 0x01, static_cast<uint32_t>(cylinders));

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

int Disk::Read(const vector<int>&, vector<uint8_t>& buf, uint64_t block)
{
	LOGTRACE("%s", __PRETTY_FUNCTION__)

	CheckReady();

	// Error if the total number of blocks is exceeded
	if (block >= GetBlockCount()) {
		 throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// leave it to the cache
	if (!cache->ReadSector(buf, static_cast<uint32_t>(block))) {
		throw scsi_exception(sense_key::MEDIUM_ERROR, asc::READ_FAULT);
	}

	return GetSectorSizeInBytes();
}

int Disk::WriteCheck(uint64_t block)
{
	CheckReady();

	// Error if the total number of blocks is exceeded
	if (block >= GetBlockCount()) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Error if write protected
	if (IsProtected()) {
		throw scsi_exception(sense_key::DATA_PROTECT, asc::WRITE_PROTECTED);
	}

	return GetSectorSizeInBytes();
}

void Disk::Write(const vector<int>&, const vector<uint8_t>& buf, uint64_t block)
{
	LOGTRACE("%s", __PRETTY_FUNCTION__)

	CheckReady();

	// Error if the total number of blocks is exceeded
	if (block >= GetBlockCount()) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
	}

	// Error if write protected
	if (IsProtected()) {
		throw scsi_exception(sense_key::DATA_PROTECT, asc::WRITE_PROTECTED);
	}

	// Leave it to the cache
	if (!cache->WriteSector(buf, static_cast<uint32_t>(block))) {
		throw scsi_exception(sense_key::MEDIUM_ERROR, asc::WRITE_FAULT);
	}
}

void Disk::Seek()
{
	CheckReady();

	EnterStatusPhase();
}

void Disk::Seek6()
{
	uint64_t start;
	if (uint32_t blocks; CheckAndGetStartAndCount(start, blocks, SEEK6)) {
		CheckReady();
	}

	EnterStatusPhase();
}

void Disk::Seek10()
{
	uint64_t start;
	if (uint32_t blocks; CheckAndGetStartAndCount(start, blocks, SEEK10)) {
		CheckReady();
	}

	EnterStatusPhase();
}

void Disk::ReadCapacity10()
{
	CheckReady();

	if (GetBlockCount() == 0) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::MEDIUM_NOT_PRESENT);
	}

	vector<uint8_t>& buf = controller->GetBuffer();

	// Create end of logical block address (blocks-1)
	uint64_t capacity = GetBlockCount() - 1;

	// If the capacity exceeds 32 bit, -1 must be returned and the client has to use READ CAPACITY(16)
	if (capacity > 4294967295) {
		capacity = -1;
	}
	SetInt32(buf, 0, static_cast<uint32_t>(capacity));

	// Create block length (1 << size)
	SetInt32(buf, 4, 1 << size_shift_count);

	// the size
	controller->SetLength(8);

	EnterDataInPhase();
}

void Disk::ReadCapacity16()
{
	CheckReady();

	if (GetBlockCount() == 0) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::MEDIUM_NOT_PRESENT);
	}

	vector<uint8_t>& buf = controller->GetBuffer();

	// Create end of logical block address (blocks-1)
	SetInt64(buf, 0, GetBlockCount() - 1);

	// Create block length (1 << size)
	SetInt32(buf, 8, 1 << size_shift_count);

	buf[12] = 0;

	// Logical blocks per physical block: not reported (1 or more)
	buf[13] = 0;

	// the size
	controller->SetLength(14);

	EnterDataInPhase();
}

void Disk::ReadCapacity16_ReadLong16()
{
	// The service action determines the actual command
	switch (controller->GetCmd(1) & 0x1f) {
	case 0x10:
		ReadCapacity16();
		break;

	case 0x11:
		ReadWriteLong16();
		break;

	default:
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
		break;
	}
}

//---------------------------------------------------------------------------
//
//	Check/Get start sector and sector count for a READ/WRITE or READ/WRITE LONG operation
//
//---------------------------------------------------------------------------

void Disk::ValidateBlockAddress(access_mode mode) const
{
	const uint64_t block = mode == RW16 ? GetInt64(controller->GetCmd(), 2) : GetInt32(controller->GetCmd(), 2);

	if (block > GetBlockCount()) {
		LOGTRACE("%s", ("Capacity of " + to_string(GetBlockCount()) + " block(s) exceeded: Trying to access block "
				+ to_string(block)).c_str())
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
	}
}

bool Disk::CheckAndGetStartAndCount(uint64_t& start, uint32_t& count, access_mode mode) const
{
	if (mode == RW6 || mode == SEEK6) {
		start = GetInt24(controller->GetCmd(), 1);

		count = controller->GetCmd(4);
		if (!count) {
			count= 0x100;
		}
	}
	else {
		start = mode == RW16 ? GetInt64(controller->GetCmd(), 2) : GetInt32(controller->GetCmd(), 2);

		if (mode == RW16) {
			count = GetInt32(controller->GetCmd(), 10);
		}
		else if (mode != SEEK6 && mode != SEEK10) {
			count = GetInt16(controller->GetCmd(), 7);
		}
		else {
			count = 0;
		}
	}

	LOGTRACE("%s READ/WRITE/VERIFY/SEEK command record=$%08X blocks=%d", __PRETTY_FUNCTION__,
			static_cast<uint32_t>(start), count)

	// Check capacity
	if (uint64_t capacity = GetBlockCount(); !capacity || start > capacity || start + count > capacity) {
		LOGTRACE("%s", ("Capacity of " + to_string(capacity) + " block(s) exceeded: Trying to access block "
				+ to_string(start) + ", block count " + to_string(count)).c_str())
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::LBA_OUT_OF_RANGE);
	}

	// Do not process 0 blocks
	if (!count && (mode != SEEK6 && mode != SEEK10)) {
		return false;
	}

	return true;
}

uint32_t Disk::CalculateShiftCount(uint32_t size_in_bytes)
{
	const auto& it = shift_counts.find(size_in_bytes);
	return it != shift_counts.end() ? it->second : 0;
}

uint32_t Disk::GetSectorSizeInBytes() const
{
	return size_shift_count ? 1 << size_shift_count : 0;
}

void Disk::SetSectorSizeInBytes(uint32_t size_in_bytes)
{
	DeviceFactory device_factory;
	if (const auto& sizes = device_factory.GetSectorSizes(GetType());
		!sizes.empty() && sizes.find(size_in_bytes) == sizes.end()) {
		throw io_exception("Invalid sector size of " + to_string(size_in_bytes) + " byte(s)");
	}

	size_shift_count = CalculateShiftCount(size_in_bytes);
	assert(size_shift_count);
}

uint32_t Disk::GetConfiguredSectorSize() const
{
	return configured_sector_size;
}

bool Disk::SetConfiguredSectorSize(const DeviceFactory& device_factory, uint32_t configured_size)
{
	if (unordered_set<uint32_t> sizes = device_factory.GetSectorSizes(GetType());
		sizes.find(configured_size) == sizes.end()) {
		return false;
	}

	configured_sector_size = configured_size;

	return true;
}
