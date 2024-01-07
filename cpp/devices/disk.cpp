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

#include "shared/piscsi_exceptions.h"
#include "scsi_command_util.h"
#include "disk.h"
#include <sstream>
#include <iomanip>

using namespace scsi_defs;
using namespace scsi_command_util;

bool Disk::Init(const param_map& params)
{
	StorageDevice::Init(params);

	// REZERO implementation is identical with Seek
	AddCommand(scsi_command::eCmdRezero, [this] { Seek(); });
	AddCommand(scsi_command::eCmdFormatUnit, [this] { FormatUnit(); });
	// REASSIGN BLOCKS implementation is identical with Seek
	AddCommand(scsi_command::eCmdReassignBlocks, [this] { Seek(); });
	AddCommand(scsi_command::eCmdRead6, [this] { Read6(); });
	AddCommand(scsi_command::eCmdWrite6, [this] { Write6(); });
	AddCommand(scsi_command::eCmdSeek6, [this] { Seek6(); });
	AddCommand(scsi_command::eCmdStartStop, [this] { StartStopUnit(); });
	AddCommand(scsi_command::eCmdPreventAllowMediumRemoval, [this]{ PreventAllowMediumRemoval(); });
	AddCommand(scsi_command::eCmdReadCapacity10, [this] { ReadCapacity10(); });
	AddCommand(scsi_command::eCmdRead10, [this] { Read10(); });
	AddCommand(scsi_command::eCmdWrite10, [this] { Write10(); });
	AddCommand(scsi_command::eCmdReadLong10, [this] { ReadWriteLong10(); });
	AddCommand(scsi_command::eCmdWriteLong10, [this] { ReadWriteLong10(); });
	AddCommand(scsi_command::eCmdWriteLong16, [this] { ReadWriteLong16(); });
	AddCommand(scsi_command::eCmdSeek10, [this] { Seek10(); });
	AddCommand(scsi_command::eCmdVerify10, [this] { Verify10(); });
	AddCommand(scsi_command::eCmdSynchronizeCache10, [this] { SynchronizeCache(); });
	AddCommand(scsi_command::eCmdSynchronizeCache16, [this] { SynchronizeCache(); });
	AddCommand(scsi_command::eCmdReadDefectData10, [this] { ReadDefectData10(); });
	AddCommand(scsi_command::eCmdRead16,[this] { Read16(); });
	AddCommand(scsi_command::eCmdWrite16, [this] { Write16(); });
	AddCommand(scsi_command::eCmdVerify16, [this] { Verify16(); });
	AddCommand(scsi_command::eCmdReadCapacity16_ReadLong16, [this] { ReadCapacity16_ReadLong16(); });

	return true;
}

void Disk::CleanUp()
{
	FlushCache();

	StorageDevice::CleanUp();
}

void Disk::Dispatch(scsi_command cmd)
{
	// Media changes must be reported on the next access, i.e. not only for TEST UNIT READY
	if (IsMediumChanged()) {
		assert(IsRemovable());

		SetMediumChanged(false);

		GetController()->Error(sense_key::unit_attention, asc::not_ready_to_ready_change);
	}
	else {
		PrimaryDevice::Dispatch(cmd);
	}
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
	if (cache != nullptr && IsReady()) {
		cache->Save();
	}
}

void Disk::FormatUnit()
{
	CheckReady();

	// FMTDATA=1 is not supported (but OK if there is no DEFECT LIST)
	if ((GetController()->GetCmdByte(1) & 0x10) != 0 && GetController()->GetCmdByte(4) != 0) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	EnterStatusPhase();
}

void Disk::Read(access_mode mode)
{
	const auto& [valid, start, blocks] = CheckAndGetStartAndCount(mode);
	if (valid) {
		GetController()->SetBlocks(blocks);
		GetController()->SetLength(Read(GetController()->GetBuffer(), start));

		LogTrace("Length is " + to_string(GetController()->GetLength()));

		// Set next block
		GetController()->SetNext(start + 1);

		EnterDataInPhase();
	}
	else {
		EnterStatusPhase();
	}
}

void Disk::ReadWriteLong10() const
{
	ValidateBlockAddress(RW10);

	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (GetInt16(GetController()->GetCmd(), 7) != 0) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	EnterStatusPhase();
}

void Disk::ReadWriteLong16() const
{
	ValidateBlockAddress(RW16);

	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (GetInt16(GetController()->GetCmd(), 12) != 0) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	EnterStatusPhase();
}

void Disk::Write(access_mode mode) const
{
	if (IsProtected()) {
		throw scsi_exception(sense_key::data_protect, asc::write_protected);
	}

	const auto& [valid, start, blocks] = CheckAndGetStartAndCount(mode);
	if (valid) {
		GetController()->SetBlocks(blocks);
		GetController()->SetLength(GetSectorSizeInBytes());

		// Set next block
		GetController()->SetNext(start + 1);

		EnterDataOutPhase();
	}
	else {
		EnterStatusPhase();
	}
}

void Disk::Verify(access_mode mode)
{
	const auto& [valid, start, blocks] = CheckAndGetStartAndCount(mode);
	if (valid) {
		// if BytChk=0
		if ((GetController()->GetCmdByte(1) & 0x02) == 0) {
			Seek();
			return;
		}

		// Test reading
		GetController()->SetBlocks(blocks);
		GetController()->SetLength(Read(GetController()->GetBuffer(), start));

		// Set next block
		GetController()->SetNext(start + 1);

		EnterDataOutPhase();
	}
	else {
		EnterStatusPhase();
	}
}

void Disk::StartStopUnit()
{
	const bool start = GetController()->GetCmdByte(4) & 0x01;
	const bool load = GetController()->GetCmdByte(4) & 0x02;

	if (load) {
		LogTrace(start ? "Loading medium" : "Ejecting medium");
	}
	else {
		LogTrace(start ? "Starting unit" : "Stopping unit");

		SetStopped(!start);
	}

	if (!start) {
		// Look at the eject bit and eject if necessary
		if (load) {
			if (IsLocked()) {
				// Cannot be ejected because it is locked
				throw scsi_exception(sense_key::illegal_request, asc::load_or_eject_failed);
			}

			// Eject
			if (!Eject(false)) {
				throw scsi_exception(sense_key::illegal_request, asc::load_or_eject_failed);
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

	const bool lock = GetController()->GetCmdByte(4) & 0x01;

	LogTrace(lock ? "Locking medium" : "Unlocking medium");

	SetLocked(lock);

	EnterStatusPhase();
}

void Disk::SynchronizeCache()
{
	FlushCache();

	EnterStatusPhase();
}

void Disk::ReadDefectData10() const
{
	const size_t allocation_length = min(static_cast<size_t>(GetInt16(GetController()->GetCmd(), 7)),
			static_cast<size_t>(4));

	// The defect list is empty
	fill_n(GetController()->GetBuffer().begin(), allocation_length, 0);
	GetController()->SetLength(static_cast<uint32_t>(allocation_length));

	EnterDataInPhase();
}

bool Disk::Eject(bool force)
{
	const bool status = PrimaryDevice::Eject(force);
	if (status) {
		FlushCache();
		cache.reset();

		// The image file for this drive is not in use anymore
		UnreserveFile();

		sector_read_count = 0;
		sector_write_count = 0;
	}

	return status;
}

int Disk::ModeSense6(cdb_t cdb, vector<uint8_t>& buf) const
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

int Disk::ModeSense10(cdb_t cdb, vector<uint8_t>& buf) const
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
	// Retry count is 0, limit time uses internal default value
	vector<byte> buf(12);

	// TB, PER, DTE (required for OpenVMS/VAX compatibility, see issue #1117)
	buf[2] = (byte)0x26;

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

int Disk::Read(span<uint8_t> buf, uint64_t block)
{
	assert(block < GetBlockCount());

	CheckReady();

	if (!cache->ReadSector(buf, static_cast<uint32_t>(block))) {
		throw scsi_exception(sense_key::medium_error, asc::read_fault);
	}

	++sector_read_count;

	return GetSectorSizeInBytes();
}

void Disk::Write(span<const uint8_t> buf, uint64_t block)
{
	assert(block < GetBlockCount());

	CheckReady();

	if (!cache->WriteSector(buf, static_cast<uint32_t>(block))) {
		throw scsi_exception(sense_key::medium_error, asc::write_fault);
	}

	++sector_write_count;
}

void Disk::Seek()
{
	CheckReady();

	EnterStatusPhase();
}

void Disk::Seek6()
{
	const auto& [valid, start, blocks] = CheckAndGetStartAndCount(SEEK6);
	if (valid) {
		CheckReady();
	}

	EnterStatusPhase();
}

void Disk::Seek10()
{
	const auto& [valid, start, blocks] = CheckAndGetStartAndCount(SEEK10);
	if (valid) {
		CheckReady();
	}

	EnterStatusPhase();
}

void Disk::ReadCapacity10()
{
	CheckReady();

	if (GetBlockCount() == 0) {
		throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);
	}

	vector<uint8_t>& buf = GetController()->GetBuffer();

	// Create end of logical block address (blocks-1)
	uint64_t capacity = GetBlockCount() - 1;

	// If the capacity exceeds 32 bit, -1 must be returned and the client has to use READ CAPACITY(16)
	if (capacity > 4294967295) {
		capacity = -1;
	}
	SetInt32(buf, 0, static_cast<uint32_t>(capacity));

	// Create block length (1 << size)
	SetInt32(buf, 4, 1 << size_shift_count);

	GetController()->SetLength(8);

	EnterDataInPhase();
}

void Disk::ReadCapacity16()
{
	CheckReady();

	if (GetBlockCount() == 0) {
		throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);
	}

	vector<uint8_t>& buf = GetController()->GetBuffer();

	// Create end of logical block address (blocks-1)
	SetInt64(buf, 0, GetBlockCount() - 1);

	// Create block length (1 << size)
	SetInt32(buf, 8, 1 << size_shift_count);

	buf[12] = 0;

	// Logical blocks per physical block: not reported (1 or more)
	buf[13] = 0;

	GetController()->SetLength(14);

	EnterDataInPhase();
}

void Disk::ReadCapacity16_ReadLong16()
{
	// The service action determines the actual command
	switch (GetController()->GetCmdByte(1) & 0x1f) {
	case 0x10:
		ReadCapacity16();
		break;

	case 0x11:
		ReadWriteLong16();
		break;

	default:
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
		break;
	}
}

void Disk::ValidateBlockAddress(access_mode mode) const
{
	const uint64_t block = mode == RW16 ? GetInt64(GetController()->GetCmd(), 2) : GetInt32(GetController()->GetCmd(), 2);

	if (block > GetBlockCount()) {
		LogTrace("Capacity of " + to_string(GetBlockCount()) + " block(s) exceeded: Trying to access block "
				+ to_string(block));
		throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
	}
}

tuple<bool, uint64_t, uint32_t> Disk::CheckAndGetStartAndCount(access_mode mode) const
{
	uint64_t start;
	uint32_t count;

	if (mode == RW6 || mode == SEEK6) {
		start = GetInt24(GetController()->GetCmd(), 1);

		count = GetController()->GetCmdByte(4);
		if (!count) {
			count= 0x100;
		}
	}
	else {
		start = mode == RW16 ? GetInt64(GetController()->GetCmd(), 2) : GetInt32(GetController()->GetCmd(), 2);

		if (mode == RW16) {
			count = GetInt32(GetController()->GetCmd(), 10);
		}
		else if (mode != SEEK6 && mode != SEEK10) {
			count = GetInt16(GetController()->GetCmd(), 7);
		}
		else {
			count = 0;
		}
	}

	stringstream s;
	s << "READ/WRITE/VERIFY/SEEK, start block: $" << setfill('0') << setw(8) << hex << start;
	LogTrace(s.str() + ", blocks: " + to_string(count));

	// Check capacity
	if (uint64_t capacity = GetBlockCount(); !capacity || start > capacity || start + count > capacity) {
		LogTrace("Capacity of " + to_string(capacity) + " block(s) exceeded: Trying to access block "
				+ to_string(start) + ", block count " + to_string(count));
		throw scsi_exception(sense_key::illegal_request, asc::lba_out_of_range);
	}

	// Do not process 0 blocks
	if (!count && (mode != SEEK6 && mode != SEEK10)) {
		return tuple(false, start, count);
	}

	return tuple(true, start, count);
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
	if (!GetSupportedSectorSizes().contains(size_in_bytes)) {
    	throw io_exception("Invalid sector size of " + to_string(size_in_bytes) + " byte(s)");
	}

	size_shift_count = CalculateShiftCount(size_in_bytes);
	assert(size_shift_count);
}

uint32_t Disk::GetConfiguredSectorSize() const
{
	return configured_sector_size;
}

bool Disk::SetConfiguredSectorSize(uint32_t configured_size)
{
	if (!supported_sector_sizes.contains(configured_size)) {
		return false;
	}

    configured_sector_size = configured_size;

	return true;
}

vector<PbStatistics> Disk::GetStatistics() const
{
	vector<PbStatistics> statistics = PrimaryDevice::GetStatistics();

	// Enrich cache statistics with device information before adding them to device statistics
	if (cache) {
		for (auto& s : cache->GetStatistics(IsReadOnly())) {
			s.set_id(GetId());
			s.set_unit(GetLun());
			statistics.push_back(s);
		}
	}

	PbStatistics s;
	s.set_id(GetId());
	s.set_unit(GetLun());

	s.set_category(PbStatisticsCategory::CATEGORY_INFO);

	s.set_key(SECTOR_READ_COUNT);
	s.set_value(sector_read_count);
	statistics.push_back(s);

	if (!IsReadOnly()) {
		s.set_key(SECTOR_WRITE_COUNT);
		s.set_value(sector_write_count);
		statistics.push_back(s);
	}

	return statistics;
}
