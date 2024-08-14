//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "scsi_command_util.h"
#include "storage_device.h"
#include <unistd.h>

using namespace std;
using namespace filesystem;
using namespace scsi_command_util;

StorageDevice::StorageDevice(PbDeviceType type, int lun, const unordered_set<uint32_t> &s)
	: ModePageDevice(type, lun)
	, supported_sector_sizes(s)
{
	SupportsFile(true);
	SetStoppable(true);
}

bool StorageDevice::Init(const param_map &pm)
{
	ModePageDevice::Init(pm);
	AddCommand(scsi_command::eCmdPreventAllowMediumRemoval, [this]{ PreventAllowMediumRemoval(); });
	return true;
}

void StorageDevice::CleanUp()
{
	UnreserveFile();

	ModePageDevice::CleanUp();
}

void StorageDevice::SetFilename(string_view f)
{
	filename = filesystem::path(f);

	// Permanently write-protected
	SetReadOnly(IsReadOnlyFile());

	SetProtectable(!IsReadOnlyFile());

	if (IsReadOnlyFile()) {
		SetProtected(false);
	}
}

void StorageDevice::ValidateFile()
{
	if (blocks == 0) {
		throw io_exception(string(GetTypeString()) + " device has 0 blocks");
	}

	if (!exists(filename)) {
		throw file_not_found_exception("Image file '" + filename.string() + "' for " + GetTypeString() + " device does not exist");
	}

	if (GetFileSize() > 2LL * 1024 * 1024 * 1024 * 1024) {
		throw io_exception("Image files > 2 TiB are not supported");
	}

	// TODO Check for duplicate handling of these properties (-> piscsi_executor.cpp)
	if (IsReadOnlyFile()) {
		// Permanently write-protected
		SetReadOnly(true);
		SetProtectable(false);
		SetProtected(false);
	}

	SetStopped(false);
	SetRemoved(false);
	SetLocked(false);
	SetReady(true);
}

void StorageDevice::ReserveFile() const
{
	assert(!filename.empty());
	assert(!reserved_files.contains(filename.string()));

	reserved_files[filename.string()] = { GetId(), GetLun() };
}

void StorageDevice::UnreserveFile()
{
	reserved_files.erase(filename.string());

	filename.clear();
}

id_set StorageDevice::GetIdsForReservedFile(const string& file)
{
	if (const auto& it = reserved_files.find(file); it != reserved_files.end()) {
		return { it->second.first, it->second.second };
	}

	return { -1, -1 };
}

uint32_t StorageDevice::CalculateShiftCount(uint32_t size_in_bytes)
{
	const auto& it = shift_counts.find(size_in_bytes);
	return it != shift_counts.end() ? it->second : 0;
}

void StorageDevice::UnreserveAll()
{
	reserved_files.clear();
}

bool StorageDevice::FileExists(string_view file)
{
	return exists(path(file));
}

bool StorageDevice::IsReadOnlyFile() const
{
	return access(filename.c_str(), W_OK);
}

off_t StorageDevice::GetFileSize() const
{
	try {
		return file_size(filename);
	}
	catch (const filesystem_error& e) {
		throw io_exception("Can't get size of '" + filename.string() + "': " + e.what());
	}
}

void StorageDevice::PreventAllowMediumRemoval()
{
	CheckReady();

	const bool lock = GetController()->GetCmdByte(4) & 0x01;

	LogTrace(lock ? "Locking medium" : "Unlocking medium");

	SetLocked(lock);

	EnterStatusPhase();
}

uint32_t StorageDevice::GetSectorSizeInBytes() const
{
	return size_shift_count ? 1 << size_shift_count : 0;
}

void StorageDevice::SetSectorSizeInBytes(uint32_t size_in_bytes)
{
	if (!GetSupportedSectorSizes().contains(size_in_bytes)) {
		throw io_exception("Invalid sector size of " + to_string(size_in_bytes) + " byte(s)");
	}

	size_shift_count = CalculateShiftCount(size_in_bytes);
	assert(size_shift_count);
}

uint32_t StorageDevice::GetMinSupportedSectorSize() const
{
	uint32_t res = 0;
	for (const auto& s : supported_sector_sizes) {
		if (!res || s < res) {
			res = s;
		}
	}
	return res;
}

uint32_t StorageDevice::GetMaxSupportedSectorSize() const
{
	uint32_t res = 0;
	for (const auto& s : supported_sector_sizes) {
		if (s > res) {
			res = s;
		}
	}
	return res;
}

uint32_t StorageDevice::GetConfiguredSectorSize() const
{
	return configured_sector_size;
}


